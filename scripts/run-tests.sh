#!/bin/bash
# GitStat test runner for Agentite
# Runs tests and outputs results in GitStat JSON format

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
RESULTS_FILE="$PROJECT_DIR/.test-results.json"

cd "$PROJECT_DIR"

# Build test runner if needed
echo "Building test runner..."
make dirs build/test_runner 2>&1 | tail -5

# Run tests with Catch2 reporter that gives us counts
echo "Running tests..."
TEST_OUTPUT=$(./build/test_runner --reporter compact 2>&1) || true

# Parse Catch2 output for results
# Catch2 compact format ends with: "Passed N test(s)..." or "Failed N test(s)..."
# Or: "test cases: N | N passed | N failed"

# Try to extract from the summary line
if echo "$TEST_OUTPUT" | grep -q "test cases:"; then
    # Format: "test cases: 201 | 200 passed | 1 failed"
    SUMMARY=$(echo "$TEST_OUTPUT" | grep "test cases:" | tail -1)
    TOTAL=$(echo "$SUMMARY" | sed -E 's/.*test cases:[[:space:]]*([0-9]+).*/\1/')
    PASSED=$(echo "$SUMMARY" | sed -E 's/.*\|[[:space:]]*([0-9]+) passed.*/\1/')
    FAILED=$(echo "$SUMMARY" | sed -E 's/.*\|[[:space:]]*([0-9]+) failed.*/\1/' || echo "0")

    # Handle "all passed" case where failed might not appear
    if ! echo "$SUMMARY" | grep -q "failed"; then
        FAILED=0
    fi
elif echo "$TEST_OUTPUT" | grep -q "All tests passed"; then
    # Format: "All tests passed (N assertions in M test cases)"
    SUMMARY=$(echo "$TEST_OUTPUT" | grep "All tests passed" | tail -1)
    TOTAL=$(echo "$SUMMARY" | sed -E 's/.*in ([0-9]+) test cases.*/\1/')
    PASSED=$TOTAL
    FAILED=0
else
    # Fallback: try to find any numbers
    TOTAL=0
    PASSED=0
    FAILED=0
fi

# Collect failure messages if any
FAILURES="[]"
if [ "$FAILED" -gt 0 ]; then
    # Extract failed test names from output
    FAILURE_LIST=$(echo "$TEST_OUTPUT" | grep -E "^.*FAILED:" | head -10 | sed 's/"/\\"/g' | awk '{printf "\"%s\",", $0}' | sed 's/,$//')
    if [ -n "$FAILURE_LIST" ]; then
        FAILURES="[$FAILURE_LIST]"
    fi
fi

# Write results JSON
cat > "$RESULTS_FILE" << EOF
{
  "passed": $PASSED,
  "failed": $FAILED,
  "total": $TOTAL,
  "failures": $FAILURES
}
EOF

echo "Test results written to $RESULTS_FILE"
echo "  Total: $TOTAL, Passed: $PASSED, Failed: $FAILED"

# Exit with appropriate code
if [ "$FAILED" -gt 0 ]; then
    exit 1
fi
exit 0
