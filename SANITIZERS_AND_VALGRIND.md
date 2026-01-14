# Sanitizers and Valgrind Usage Guide

This document describes how to use the sanitizer and Valgrind support in DTLMod for detecting memory leaks, undefined behavior, and other runtime issues.

## Building with Sanitizers

DTLMod supports several sanitizers through CMake options:

### AddressSanitizer (ASan)
Detects memory errors like buffer overflows, use-after-free, and memory leaks.

```bash
cmake -B build -DENABLE_ASAN=ON
cmake --build build
```

### UndefinedBehaviorSanitizer (UBSan)
Detects undefined behavior like integer overflow, null pointer dereference, etc.

```bash
cmake -B build -DENABLE_UBSAN=ON
cmake --build build
```

### ThreadSanitizer (TSan)
Detects data races and other threading issues.

```bash
cmake -B build -DENABLE_TSAN=ON
cmake --build build
```

### Combining Sanitizers
You can enable multiple sanitizers (except TSan, which conflicts with ASan):

```bash
cmake -B build -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
cmake --build build
```

### Running Tests with Sanitizers

After building with sanitizers enabled, run tests normally:

```bash
cd build
./unit_tests
```

Sanitizer output will be printed to stderr. You can control sanitizer behavior with environment variables:

```bash
# ASan options
ASAN_OPTIONS=detect_leaks=1:check_initialization_order=1 ./unit_tests

# UBSan options
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ./unit_tests
```

## Building with Valgrind Support

Valgrind is incompatible with sanitizers, so use a separate build:

```bash
cmake -B build -DENABLE_VALGRIND=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target unit_tests
```

### Running Valgrind

Use the built-in Valgrind target:

```bash
cd build
make valgrind
```

This will run the unit tests under Valgrind and create a detailed report in `valgrind-output.txt`.

### Valgrind Output

The Valgrind report includes:
- Memory leaks (definitely lost, indirectly lost, possibly lost)
- Invalid memory accesses
- Use of uninitialized values
- Source location of errors

### Customizing Valgrind Suppressions

Edit `test/valgrind.supp` to suppress known false positives from third-party libraries.

## CI/CD Integration

### Main Build Workflow

The main build workflow ([.github/workflows/build.yml](.github/workflows/build.yml)) has been optimized:
- ✅ Added dependency caching to speed up builds (SimGrid and FSMod with Python bindings, GTest cached in `/opt/`)
- ✅ Stable cache keys using version numbers (e.g., `simgrid-python-v2`) instead of workflow file hashes
- ✅ Cache persists across workflow changes - only increment version when dependencies need rebuilding
- ✅ Consolidated duplicate build steps
- ✅ Uses modern CMake commands
- ✅ Builds only once instead of twice
- ✅ Conditional execution of expensive steps
- ✅ Proper CMAKE_PREFIX_PATH and PYTHONPATH configuration for cached dependencies
- ✅ SimGrid and FSMod built with Python bindings enabled for full Python test support
- ✅ Dynamic detection of Python module installation paths

**Cache Strategy:**
- Dependencies are cached with static version numbers (v1, v2, etc.)
- To invalidate cache and rebuild a dependency, increment its version number
- This prevents unnecessary rebuilds when only the workflow file changes

### Weekly Sanitizer & Valgrind Checks

A new weekly workflow ([.github/workflows/weekly-checks.yml](.github/workflows/weekly-checks.yml)) runs comprehensive checks:
- Runs every Monday at 3 AM UTC
- Can be manually triggered via workflow_dispatch
- Executes tests with multiple sanitizers:
  - AddressSanitizer
  - UndefinedBehaviorSanitizer
  - AddressSanitizer + UndefinedBehaviorSanitizer
  - Valgrind
- Generates reports and uploads artifacts
- Posts commit comments on failures
- Reports are retained for 30 days (90 days for summary)

### Viewing Reports

1. Go to the Actions tab in GitHub
2. Select "Weekly Memory & Sanitizer Checks"
3. Click on the latest workflow run
4. Download artifacts to view detailed reports:
   - `AddressSanitizer-report`
   - `UndefinedBehaviorSanitizer-report`
   - `valgrind-report`
   - `valgrind-summary`
   - `weekly-report` (overview of all checks)

## Troubleshooting

### Common Issues

**Sanitizer reports too many false positives from libraries:**
- Use suppression files or environment variables to filter
- Consider using `ASAN_OPTIONS=detect_leaks=0` to disable leak checking

**Valgrind is very slow:**
- This is expected; Valgrind instruments every memory operation
- Run it only on CI or for specific debugging
- Use `--leak-check=summary` for faster runs

**Sanitizers crash on initialization:**
- Ensure all dependencies are built without sanitizers
- Some libraries may be incompatible with sanitizers

**Build fails with both Valgrind and sanitizers enabled:**
- This is intentional; they cannot be used together
- Choose one or the other for each build

## Best Practices

1. **Development**: Use sanitizers for quick feedback during development
2. **CI**: Run regular sanitizer checks on every PR
3. **Weekly**: Deep Valgrind analysis for comprehensive leak detection
4. **Before release**: Run all sanitizers and Valgrind checks
5. **Debug build**: Always use Debug builds with sanitizers/Valgrind for better stack traces

## Performance Impact

| Tool | Runtime Overhead | Memory Overhead | Use Case |
|------|------------------|-----------------|----------|
| ASan | 2x | 3x | Development, CI |
| UBSan | ~20% | Minimal | Development, CI |
| TSan | 5-15x | 5-10x | Threading issues |
| Valgrind | 10-50x | Minimal | Deep analysis |

## Additional Resources

- [AddressSanitizer Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [UndefinedBehaviorSanitizer Documentation](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [ThreadSanitizer Documentation](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)
- [Valgrind Documentation](https://valgrind.org/docs/manual/manual.html)
