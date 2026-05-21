#!/usr/bin/env python3
"""
Build unit tests for Buddy firmware

Simple wrapper that handles:
- CMake configuration
- Parallel ninja builds with auto-detected core count
- Building specific test targets
- Code coverage reporting

After building, run tests manually with ctest:
    cd build_tests && ctest
    cd build_tests && ctest -R gcode  # run specific tests
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import webbrowser
from pathlib import Path
import tree_sitter_cpp
from tree_sitter import Language, Parser


def find_project_root():
    """Find the project root directory."""
    script_dir = Path(__file__).parent.resolve()
    return script_dir.parent


def setup_tool_from_bootstrap(tool_name, project_root):
    """Try to setup a tool from bootstrap.py dependencies."""
    bootstrap_py = project_root / 'utils' / 'bootstrap.py'

    if not bootstrap_py.exists():
        return False

    try:
        result = subprocess.run([
            sys.executable,
            str(bootstrap_py), '--print-dependency-directory', tool_name
        ],
                                capture_output=True,
                                text=True,
                                check=False)

        if result.returncode != 0:
            return False

        tool_dir = Path(result.stdout.strip())
        if tool_name == 'cmake':
            tool_dir = tool_dir / 'bin'

        if tool_dir.exists():
            os.environ['PATH'] = f"{tool_dir}:{os.environ['PATH']}"
            return True
    except Exception:
        pass

    return False


def check_tool_in_system(tool_name):
    """
    Check if a tool is available on the system

    Returns True if tool is available, False otherwise.
    """
    return subprocess.run(['which', tool_name],
                          capture_output=True).returncode == 0


def check_tool(tool_name, project_root):
    """
    Check if a tool is available, trying bootstrap if not found.

    Prioritizes project dependencies (.dependencies) over system tools
    for reproducible builds.

    Returns True if tool is available, False otherwise.
    Prints error message if tool cannot be found.
    """
    # Try bootstrap/.dependencies first (prepends to PATH, takes priority)
    if setup_tool_from_bootstrap(tool_name, project_root):
        if subprocess.run(['which', tool_name],
                          capture_output=True).returncode == 0:
            return True

    # Fall back to whatever is already on the system PATH
    if check_tool_in_system(tool_name):
        return True

    # Tool not found, print error
    print(f"Error: {tool_name} not found", file=sys.stderr)
    print(f"Please install {tool_name} or run: python3 utils/bootstrap.py",
          file=sys.stderr)
    return False


def run_command(cmd, verbose=False):
    """Run a shell command."""
    if verbose:
        print(f"$ {' '.join(str(c) for c in cmd)}")

    result = subprocess.run(cmd, capture_output=not verbose, text=True)

    if result.returncode != 0:
        if not verbose and result.stderr:
            print(result.stderr, file=sys.stderr)
        sys.exit(result.returncode)

    return result


def get_core_count():
    """Get number of CPU cores for parallel builds."""
    return os.cpu_count() or 4


COVERAGE_SOURCE_DIRS = [
    'src',
    Path('lib') / 'Marlin',
    Path('lib') / 'WUI',
    Path('lib') / 'openprinttag',
    Path('lib') / 'Prusa-Firmware-MMU' / 'src',
]


def collect_source_files(project_root):
    """Collect all source files in coverage-tracked directories."""
    source_extensions = {
        '.c', '.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx'
    }
    files = set()
    for rel_dir in COVERAGE_SOURCE_DIRS:
        src_dir = project_root / rel_dir
        if src_dir.is_dir():
            for f in src_dir.rglob('*'):
                if f.suffix in source_extensions and f.is_file():
                    files.add(str(f.relative_to(project_root)))
    return files


def function_name(func_def_node, source):
    """Extract a function's name from its function_definition node."""
    declarator = func_def_node.child_by_field_name('declarator')
    # Drill through pointer_declarator / reference_declarator wrappers.
    while declarator is not None and declarator.type != 'function_declarator':
        inner = declarator.child_by_field_name('declarator')
        if inner is None or inner is declarator:
            return None
        declarator = inner
    if declarator is None:
        return None
    name_node = declarator.child_by_field_name('declarator')
    if name_node is None:
        return None
    return source[name_node.start_byte:name_node.end_byte] \
        .decode('utf-8', errors='replace')


def is_short_circuit(node):
    """True if node is a `&&` or `||` binary expression."""
    if node.type != 'binary_expression':
        return False
    op = node.child_by_field_name('operator')
    return op is not None and op.text in (b'&&', b'||')


def is_code_line(stripped):
    """True if a stripped line looks like executable code."""
    if not stripped:
        return False
    if stripped.startswith(('//', '*', '#')):
        return False
    return stripped not in {'{', '}', '};', '})', '});'}


def parse_source_file(filepath):
    """
    Extract functions, branch points, and executable lines from a C/C++
    file to build a synthetic 0-count coverage baseline.
    """
    try:
        with open(filepath, 'rb') as f:
            source = f.read()
    except OSError:
        return None
    if not source:
        return None

    parser = Parser(Language(tree_sitter_cpp.language()))
    tree = parser.parse(source)

    branch_nodes = {
        'if_statement',
        'while_statement',
        'for_statement',
        'for_range_loop',
        'do_statement',
        'conditional_expression',
        'case_statement',
    }

    # Shape matches what gcov emits for a 2-way conditional.
    branch_pair = [
        {
            "blockno": 0,
            "count": 0,
            "fallthrough": True,
            "throw": False
        },
        {
            "blockno": 0,
            "count": 0,
            "fallthrough": False,
            "throw": False
        },
    ]

    functions = []
    branch_lines = []

    stack = [tree.root_node]
    while stack:
        node = stack.pop()
        if node.type == 'function_definition':
            name = function_name(node, source)
            if name:
                functions.append({
                    "name": name,
                    "lineno": node.start_point[0] + 1,
                    "execution_count": 0,
                    "returned_count": 0,
                    "blocks_percent": 0.0,
                })
        elif node.type in branch_nodes or is_short_circuit(node):
            branch_lines.append(node.start_point[0] + 1)
        stack.extend(node.children)

    text = source.decode('utf-8', errors='replace')
    code_line_numbers = []
    in_block_comment = False
    for i, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if in_block_comment:
            if '*/' in stripped:
                in_block_comment = False
            continue
        if stripped.startswith('/*') and '*/' not in stripped[2:]:
            in_block_comment = True
            continue
        if is_code_line(stripped):
            code_line_numbers.append(i)

    branch_count_per_line = {}
    for ln in branch_lines:
        branch_count_per_line[ln] = branch_count_per_line.get(ln, 0) + 1

    all_lines = sorted(set(code_line_numbers) | set(branch_count_per_line))
    lines = [{
        "line_number": ln,
        "count": 0,
        "branches": branch_pair * branch_count_per_line.get(ln, 0),
    } for ln in all_lines]

    return {"lines": lines, "functions": functions}


def generate_zero_coverage_tracefile(project_root, real_json_path,
                                     output_path):
    """
    Build a gcovr tracefile that supplements `real_json_path`:

    - source files not compiled into any test become 0% entries
    - headers already in real coverage get their full line count restored
      (gcov sees only the few lines that got instantiated)

    Returns the list of files not compiled into any test.
    """
    with open(real_json_path) as f:
        real_data = json.load(f)

    covered_files = {entry['file'] for entry in real_data.get('files', [])}
    all_files = collect_source_files(project_root)

    tracefile = {
        "gcovr/format_version": real_data.get("gcovr/format_version", "0.6"),
        "files": [],
    }
    not_compiled = []
    header_extensions = {'.h', '.hpp', '.hh', '.hxx'}

    for filepath in sorted(all_files):
        abs_path = project_root / filepath
        is_header = abs_path.suffix in header_extensions
        is_in_real = filepath in covered_files

        # Compiled non-header files have accurate gcov data; don't shadow it.
        if is_in_real and not is_header:
            continue

        parsed = parse_source_file(abs_path)
        if parsed is None or not parsed["functions"]:
            continue

        tracefile["files"].append({
            "file": filepath,
            "lines": parsed["lines"],
            "functions": parsed["functions"],
        })
        if not is_in_real:
            not_compiled.append(filepath)

    with open(output_path, 'w') as f:
        json.dump(tracefile, f)

    total_funcs = sum(len(f["functions"]) for f in tracefile["files"])
    headers_supplemented = len(tracefile["files"]) - len(not_compiled)
    print(f"  {len(not_compiled)} files not compiled into any test"
          f" (~{total_funcs} functions in synthetic baseline);"
          f" supplemented {headers_supplemented} partially-reported headers")
    return not_compiled


def patch_html(html_path, not_compiled_files):
    """Fade the file icon in rows for files not compiled into any test."""
    row_marker = '<div class="Box-row'
    row_file_re = re.compile(r'title="([^"]+)"')
    row_icon_open_re = re.compile(
        r'<svg [^>]*?class="color-fg-muted"[^>]*?style="')
    if not not_compiled_files:
        return
    with open(html_path) as f:
        html = f.read()
    not_compiled_set = set(not_compiled_files)
    parts = html.split(row_marker)
    for i in range(1, len(parts)):
        file_match = row_file_re.search(parts[i])
        if file_match and file_match.group(1) in not_compiled_set:
            parts[i] = row_icon_open_re.sub(
                lambda m: m.group(0) + 'opacity:0.5;', parts[i], count=1)
    with open(html_path, 'w') as f:
        f.write(row_marker.join(parts))


def generate_coverage_report(project_root, build_dir):
    """Generate text + HTML coverage reports using gcovr."""
    # HTML post-processing in patch_html() depends on gcovr's exact output
    version = subprocess.check_output(['gcovr', '--version'],
                                      text=True).split('\n')[0]
    assert (version == 'gcovr 7.2')

    gcovr_base = [
        'gcovr',
        '--root',
        str(project_root),
        str(build_dir),
        '--exclude-unreachable-branches',
        '--exclude-throw-branches',
        '--merge-mode-functions=merge-use-line-min',
    ]
    for rel_dir in COVERAGE_SOURCE_DIRS:
        gcovr_base += ['--filter', str(project_root / rel_dir)]

    real_json = build_dir / 'coverage_real.json'
    print("Collecting coverage data...")
    run_command(gcovr_base + ['--json', '-o', str(real_json)])

    zero_json = build_dir / 'coverage_zero.json'
    not_compiled_files = generate_zero_coverage_tracefile(
        project_root, real_json, zero_json)

    gcovr_report = [
        'gcovr',
        '--root',
        str(project_root),
        '--add-tracefile',
        str(real_json),
        '--merge-mode-functions=merge-use-line-min',
    ]
    if not_compiled_files:
        gcovr_report += ['--add-tracefile', str(zero_json)]

    print()
    run_command(
        gcovr_report +
        ['--txt', str(build_dir / 'coverage.txt'), '--txt-summary'],
        verbose=True)
    print("Note: values may be inaccurate due to the used heuristics.")

    html_dir = build_dir / 'coverage_html'
    html_dir.mkdir(exist_ok=True)
    index_path = html_dir / 'index.html'
    print(f"\nGenerating HTML report in {html_dir}/")
    run_command(gcovr_report + [
        '--html-details',
        str(index_path),
        '--html-theme',
        'github.dark-green',
    ])
    patch_html(index_path, not_compiled_files)
    print(f"Opening: {index_path}")
    if not webbrowser.open(index_path.resolve().as_uri()):
        print(f"  Could not auto-open the report; open it manually.",
              file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description='Build unit tests for Buddy firmware',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                           # Build all tests
  %(prog)s cobs_tests                # Build only cobs tests
  %(prog)s --list                    # List available test targets
  %(prog)s --debug                   # Build with debug symbols
  %(prog)s --release                 # Build with release flags (-DNDEBUG)
  %(prog)s --jobs 8                  # Use 8 parallel jobs
  %(prog)s --rebuild                 # Clean rebuild
  %(prog)s --build-dir my_tests      # Use custom build directory
  %(prog)s --run                     # Build and run all tests
  %(prog)s --run -- -R gcode         # Build and run tests matching 'gcode'
  %(prog)s --run -- -LE slow         # Build and run, excluding slow tests
  %(prog)s --test                    # Run tests only (skip build)
  %(prog)s -t -- -R gcode            # Run only gcode tests (skip build)
  %(prog)s -t -- -LE slow --verbose  # Run fast tests with verbose output
  %(prog)s --coverage                # Build, run tests, generate coverage report
""")

    parser.add_argument(
        'targets',
        nargs='*',
        help=
        'Specific test targets to build (e.g., cobs_tests). If empty, builds all tests.'
    )
    parser.add_argument('--build-dir',
                        default='build_tests',
                        help='Build directory name (default: build_tests)')
    parser.add_argument('--board',
                        default='BUDDY',
                        help='Board to build for (default: BUDDY)')
    parser.add_argument(
        '--debug',
        action='store_true',
        help='Build with debug symbols (-DCMAKE_BUILD_TYPE=Debug)')
    parser.add_argument(
        '--release',
        action='store_true',
        help='Build with optimizations (-DCMAKE_BUILD_TYPE=Release)')
    parser.add_argument('--rebuild',
                        action='store_true',
                        help='Clean and rebuild from scratch')
    parser.add_argument('--clean',
                        action='store_true',
                        help='Remove build directory and exit')
    parser.add_argument(
        '-j',
        '--jobs',
        type=int,
        default=get_core_count(),
        help=
        f'Number of parallel build jobs (default: auto-detect, currently {get_core_count()})'
    )
    parser.add_argument('-v',
                        '--verbose',
                        action='store_true',
                        help='Verbose output')
    parser.add_argument('--list',
                        action='store_true',
                        help='List available test targets and exit')
    parser.add_argument(
        '--run',
        '-r',
        action='store_true',
        help='Build and run tests. Use -- to pass args to ctest.')
    parser.add_argument(
        '--test',
        '-t',
        action='store_true',
        help='Run tests only (skip build). Use -- to pass args to ctest.')
    parser.add_argument(
        '--coverage',
        action='store_true',
        help=
        'Generate HTML coverage report. Uses a separate build directory (build_tests_coverage by default).'
    )

    # Manually split argv on '--' to separate script args from ctest args
    if '--' in sys.argv:
        split_idx = sys.argv.index('--')
        script_args = sys.argv[1:split_idx]
        ctest_args = sys.argv[split_idx + 1:]
    else:
        script_args = sys.argv[1:]
        ctest_args = []

    args = parser.parse_args(script_args)

    # Validate mutually exclusive options
    if args.debug and args.release:
        print("Error: --debug and --release are mutually exclusive",
              file=sys.stderr)
        sys.exit(1)
    if args.coverage and args.release:
        print("Error: --coverage and --release are mutually exclusive",
              file=sys.stderr)
        sys.exit(1)

    project_root = find_project_root()

    # Coverage builds must not share a build dir with regular builds, since
    # the instrumented object files would otherwise be reused for non-coverage
    # builds (and vice versa).
    if args.coverage and args.build_dir == 'build_tests':
        args.build_dir = 'build_tests_coverage'

    build_dir = project_root / args.build_dir

    # Handle --test (run only, skip build)
    if args.test:
        if not build_dir.exists():
            print(f"Error: Build directory '{build_dir}' does not exist.",
                  file=sys.stderr)
            print("Run without --test first to build the tests.",
                  file=sys.stderr)
            sys.exit(1)

        cmake_cache = build_dir / 'CMakeCache.txt'
        if not cmake_cache.exists():
            print(f"Error: Build directory '{build_dir}' is not configured.",
                  file=sys.stderr)
            print("Run without --test first to configure and build.",
                  file=sys.stderr)
            sys.exit(1)

        # Ensure cmake/ctest is available
        if not check_tool('cmake', project_root):
            sys.exit(1)

        os.chdir(build_dir)
        ctest_cmd = ['ctest'] + ctest_args
        print(f"Running tests in {build_dir} with: \"{" ".join(ctest_cmd)}\"") #yapf: disable
        if args.verbose:
            print(f"$ {' '.join(ctest_cmd)}")
        result = subprocess.run(ctest_cmd)
        sys.exit(result.returncode)

    # Handle --clean and --rebuild
    if args.clean or args.rebuild:
        print("Cleaning build directory...")
        shutil.rmtree(build_dir, ignore_errors=True)
        if args.clean:
            return 0

    # Check for required tools (will attempt bootstrap if needed)
    if not check_tool('cmake', project_root):
        sys.exit(1)

    if not check_tool('ninja', project_root):
        sys.exit(1)

    # Create build directory if needed
    build_dir.mkdir(exist_ok=True)

    # Change to build directory
    os.chdir(build_dir)

    # Configure with CMake if needed
    cmake_cache = build_dir / 'CMakeCache.txt'
    if not cmake_cache.exists():
        build_type = 'Debug' if args.debug or args.coverage else (
            'Release' if args.release else 'default')
        coverage_str = ', Coverage: ON' if args.coverage else ''
        print(
            f"Configuring tests (Board: {args.board}, Build type: {build_type}{coverage_str})..."
        )

        cmake_args = [
            'cmake',
            str(project_root),
            '-G',
            'Ninja',
            f'-DBOARD={args.board}',
        ]

        if args.coverage:
            # Coverage needs debug info and no optimization for accurate results
            cmake_args.extend([
                '-DCOVERAGE_ENABLE=ON',
                '-DCMAKE_BUILD_TYPE=Debug',
            ])
        elif args.debug:
            cmake_args.append('-DCMAKE_BUILD_TYPE=Debug')
        elif args.release:
            cmake_args.append('-DCMAKE_BUILD_TYPE=Release')
        # else: default (no CMAKE_BUILD_TYPE) - assertions active (no -DNDEBUG), -Os optimization

        run_command(cmake_args, verbose=args.verbose)
        print()

    # Handle --list
    if args.list:
        print("Available test targets:")
        result = subprocess.run(['ninja', '-t', 'query', 'tests/unit/tests'],
                                capture_output=True,
                                text=True)
        for line in result.stdout.split('\n'):
            target = Path(line).name
            print(target)
        return 0

    # Determine number of jobs
    jobs = args.jobs

    # Build tests
    ninja_args = ['ninja', f'-j{jobs}']

    if args.targets:
        # Build specific targets
        ninja_args.extend(args.targets)
        print(
            f"Building specific tests: {', '.join(args.targets)} ({jobs} parallel jobs)..."
        )
    else:
        # Build all tests
        ninja_args.append('tests')
        print(f"Building all tests ({jobs} parallel jobs)...")

    # Run ninja with output visible to show progress
    if args.verbose:
        ninja_args.append('-v')
        print(f"$ {' '.join(ninja_args)}")

    result = subprocess.run(ninja_args)
    if result.returncode != 0:
        sys.exit(result.returncode)

    print()
    print("Build complete!")

    if args.coverage:
        if not check_tool('gcovr', project_root):
            sys.exit(1)

        ctest_cmd = ['ctest'] + ctest_args
        print(f"Running tests: {' '.join(ctest_cmd)}")
        result = subprocess.run(ctest_cmd)
        if result.returncode != 0:
            print("Warning: some tests failed, generating coverage anyway",
                  file=sys.stderr)

        print()
        generate_coverage_report(project_root, build_dir)

    elif args.run:
        # ctest is part of cmake, so it should be available after check_tool('cmake')
        ctest_cmd = ['ctest'] + ctest_args
        print(f"Running tests: {' '.join(ctest_cmd)}")
        result = subprocess.run(ctest_cmd)
        sys.exit(result.returncode)

    else:
        print()
        print("Run tests with:")
        print(f"  cd {build_dir} && ctest")
        print(f"  cd {build_dir} && ctest -R <pattern>")
        print(f"  cd {build_dir} && ctest --verbose")

    return 0


if __name__ == '__main__':
    sys.exit(main())
