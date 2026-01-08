#!/bin/bash
# Sanitizer test runner for Agentite
# Runs tests with AddressSanitizer and UndefinedBehaviorSanitizer
#
# Usage:
#   ./scripts/run-sanitizers.sh          # Run all sanitizer tests
#   ./scripts/run-sanitizers.sh verbose  # Run with verbose output
#   ./scripts/run-sanitizers.sh quick    # Run a subset of tests (faster)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# Parse arguments
VERBOSE=""
QUICK=""
for arg in "$@"; do
    case "$arg" in
        verbose) VERBOSE="--success" ;;
        quick) QUICK="[unit]" ;;  # Run only unit tests for quick check
        *) echo "Unknown argument: $arg"; exit 1 ;;
    esac
done

echo "=============================================="
echo "Running Agentite tests with sanitizers"
echo "=============================================="
echo ""

# Check platform for ASAN options
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS: LeakSanitizer not supported
    export ASAN_OPTIONS="abort_on_error=1:halt_on_error=1:print_stats=1"
else
    # Linux: Full ASAN with leak detection
    export ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:halt_on_error=1:print_stats=1"
fi

# UBSAN options - make undefined behavior fatal
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"

echo "Building with AddressSanitizer + UndefinedBehaviorSanitizer..."
echo "ASAN_OPTIONS: $ASAN_OPTIONS"
echo "UBSAN_OPTIONS: $UBSAN_OPTIONS"
echo ""

# Build sanitizer test runner
make asan-dirs build/asan/test_runner

echo ""
echo "Running sanitizer tests..."
echo ""

# Run tests
if [ -n "$VERBOSE" ]; then
    ./build/asan/test_runner $VERBOSE $QUICK
else
    ./build/asan/test_runner $QUICK
fi

EXIT_CODE=$?

echo ""
echo "=============================================="
if [ $EXIT_CODE -eq 0 ]; then
    echo "All sanitizer tests passed!"
else
    echo "Sanitizer tests failed with exit code $EXIT_CODE"
fi
echo "=============================================="

exit $EXIT_CODE
