# How to run unit tests?

## Quick Start (Recommended)

Use the automated build script for streamlined building and running tests:

```bash
# Build all tests (uses all CPU cores automatically)
python3 utils/build_tests.py

# Build and run all tests
python3 utils/build_tests.py --run

# Build and run, excluding slow tests
python3 utils/build_tests.py --run -- -LE slow

# Run tests only (skip build) - can be run from anywhere
python3 utils/build_tests.py --test
python3 utils/build_tests.py -t -- -LE slow           # Run fast tests only
python3 utils/build_tests.py -t -- -R gcode           # Run gcode tests only (based on Catch2 labels in test source files)
```

### Build Options

```bash
# Build specific test targets only
python3 utils/build_tests.py cobs_tests ring_allocator_tests

# Build with debug symbols (for GDB debugging)
python3 utils/build_tests.py --debug

# Build with release flags (disables assertions via -DNDEBUG)
python3 utils/build_tests.py --release

# List available test targets
python3 utils/build_tests.py --list

# Clean rebuild
python3 utils/build_tests.py --rebuild

# Use custom number of parallel jobs
python3 utils/build_tests.py --jobs 8
```

### Build Types Explained

| Build Type | Flag | Optimization | Assertions | Use Case |
|------------|------|--------------|------------|----------|
| **Default** | (none) | `-Os` | Active | Recommended for development |
| **Debug** | `--debug` | `-Og` | Active | Debugging with GDB |
| **Release** | `--release` | `-Os` + `-O3` | Disabled (`-DNDEBUG`) | Production-like testing |

> **Note:** The default build type (no flag) keeps assertions active while using release-level optimization.
> This catches bugs via `assert()` while still being fast. Use `--release` only when you specifically
> need to test with assertions disabled.

### Running Tests with ctest Arguments

Use `--` to pass arguments directly to ctest:

```bash
# Build and run with ctest args
python3 utils/build_tests.py --run -- -R gcode         # Tests matching 'gcode'
python3 utils/build_tests.py --run -- -LE slow         # Exclude slow tests
python3 utils/build_tests.py --run -- --verbose        # Verbose output

# Run only (skip build) with ctest args
python3 utils/build_tests.py -t -- -R gcode -LE slow   # Combine filters
python3 utils/build_tests.py -t -- --rerun-failed      # Re-run failed tests
```

> The script automatically handles CMake/Ninja from bootstrap.py if not found on system.

> It is recommended to use GCC for compiling unit tests.

## Running Specific Tests (Recommended for Fast Iteration)

**You don't need to run all tests every time!** CTest provides powerful filtering:

```bash
# Run tests by name pattern (regex)
ctest -R gcode                    # Run all tests with "gcode" in the name
ctest -R "gcode|json"             # Run gcode OR json tests
ctest -R "^gcode_parser"          # Tests starting with "gcode_parser"

# Run tests by label/tag
ctest -L translator               # Run only tests tagged with [translator]
ctest -L "GcodeReader"            # Run only GcodeReader tests

# Exclude tests by label
ctest -LE slow                    # Skip slow tests (recommended)

# Combine filters
ctest -R gcode -LE slow           # Run gcode tests but skip slow ones
ctest -L translator --verbose     # Run translator tests with verbose output
```

**Common workflows:**
- **Daily development:** `python3 utils/build_tests.py -t -- -LE slow` (fast tests only, reduce execution time by ~90%)
- **Testing specific feature:** `python3 utils/build_tests.py -t -- -R <feature_name>`
- **Before committing:** `python3 utils/build_tests.py --run` (full suite)

## Code Coverage

Generate HTML coverage report:

```bash
# Build instrumented tests, run them, and open the HTML report
python3 utils/build_tests.py --coverage

# Pass ctest filters to run coverage on a subset of tests
python3 utils/build_tests.py --coverage -- -R gcode
```

Coverage builds use a separate build directory (`build_tests_coverage`) so they don't interfere with regular test builds.

## Manual Building (Alternative)

If you prefer to build manually or need more control:

```bash
# Create build folder and run cmake
mkdir -p build_tests && cd build_tests
cmake .. -G Ninja -DBOARD=BUDDY

# Build all unit tests
ninja tests
```

> In case you don't have sufficient CMake or Ninja installed, you can use the ones downloaded by bootstrap.py:
> ```bash
> export PATH="$(python ../utils/bootstrap.py --print-dependency-directory cmake)/bin:$PATH"
> export PATH="$(python ../utils/bootstrap.py --print-dependency-directory ninja):$PATH"
> ```

### Running Tests Manually

```bash
# Using CTest
ctest

# Using CMake directly
cmake --build . --target test

# Using Ninja
ninja test
```

### Useful CTest Flags

- `--output-on-failure`: Show test output only when tests fail (default behavior)
- `--verbose`: Always show all test output
- `--rerun-failed`: Re-run only tests that failed last time
- `-N`: List tests that would run without actually running them

Example:
```bash
ctest -R gcode -LE slow --verbose          # Run gcode tests, skip slow, verbose output
ctest -N                                    # List all tests without running
ctest -LE slow -N                          # List fast tests only
```

### Building with Debug Symbols

To enable debugging, build with the debug flag:

```bash
# Using the build script
python3 utils/build_tests.py --debug

# Or manually
cmake .. -G Ninja -DBOARD=BUDDY -DCMAKE_BUILD_TYPE=Debug
```

### Debugging with GDB

You can debug tests using GDB, but the approach depends on whether the test has external dependencies:

#### For simple tests (no external dependencies):
Run GDB directly from the main project folder:
```bash
gdb ./build_tests/tests/unit/path/to/test_executable
```

#### For tests with external dependencies:
These tests must be run from their executable's directory to properly locate dependencies.

1. Navigate to the executable's directory:
```bash
cd build_tests/tests/unit/common/gcode/reader
```

2. Start GDB and specify the source directory and the test executable:
```bash
gdb -d <path_to_buddy> test_executable
```

> Tip: Run GDB with `-tui` flag for a nicer interface.

## How to create a new unit test?

1. Create a corresponding directory for it.
    - For example, for a unit in `src/guiapi/src/gui_timer.c` create directory `tests/unit/guiapi/gui_timer`.
2. Store your unittest cases within this directory together with their dependencies.
    Don't use the same file name for testing file and source file. Use '.cpp' extension.
3. Add a CMakeLists.txt with description on how to build your tests.
    - See other unit tests for examples.
    - Don't forget to register any directory you add using `add_subdirectory` in CMakeLists.txt in the same directory.

## Tests on Windows

1. Download & install MinGW and make sure .../MinGW/bin/ is in your path.
2. Check if Python is installed.
3. Download & install some bash (GIT bash could be already installed).
4. Run bash and get to your repository directory (cd ...).
5. Run these to prepare for test:

```bash
mkdir -p build_tests \
&& cd build_tests \
&& rm -rf * \
&& export PATH="$(python ../utils/bootstrap.py --print-dependency-directory cmake)/bin:$PATH" \
&& export PATH="$(python ../utils/bootstrap.py --print-dependency-directory ninja):$PATH" \
&& export CTEST_OUTPUT_ON_FAILURE=1 \
&& cmake .. -G Ninja
```
