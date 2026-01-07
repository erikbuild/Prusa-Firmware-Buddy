# Manual tests

These tests cover cases that are either fuzzy or they operate on a large set —
e.g., signal processing, calibration procedures, source data classification.
Hence, each test is a separate binary that produces data consumed by a Python
script that transforms the output into human-digestible form (e.g., a plot).

These tests are built under CI, but they are not run. They serve mainly as a
debugging aid and the CI checks that they do not become uncompilable in the
future.
