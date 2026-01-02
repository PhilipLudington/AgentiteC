#!/bin/bash
# GitStat build runner for Agentite
# Runs build and outputs results in GitStat JSON format

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# Determine build type
BUILD_TYPE="${1:-release}"

if [ "$BUILD_TYPE" = "debug" ]; then
    RESULTS_FILE="$PROJECT_DIR/.debug-build-results.json"
    BUILD_CMD="make DEBUG=1"
    echo "Running debug build..."
else
    RESULTS_FILE="$PROJECT_DIR/.build-results.json"
    BUILD_CMD="make"
    echo "Running release build..."
fi

# Clean first for fresh build status
make clean > /dev/null 2>&1 || true

# Run build and capture output
BUILD_OUTPUT=$($BUILD_CMD 2>&1) || BUILD_FAILED=1

# Count errors and warnings from compiler output
ERRORS=$(echo "$BUILD_OUTPUT" | grep -c ": error:" 2>/dev/null) || ERRORS=0
WARNINGS=$(echo "$BUILD_OUTPUT" | grep -c ": warning:" 2>/dev/null) || WARNINGS=0

# Ensure we have valid integers (strip whitespace)
ERRORS=${ERRORS//[^0-9]/}
WARNINGS=${WARNINGS//[^0-9]/}
: ${ERRORS:=0}
: ${WARNINGS:=0}

# Determine success
if [ "${BUILD_FAILED:-0}" = "1" ] || [ "$ERRORS" -gt 0 ]; then
    SUCCESS="false"
else
    SUCCESS="true"
fi

# Collect error/warning messages (first 10)
MESSAGES="[]"
MSG_LIST=$(echo "$BUILD_OUTPUT" | grep -E ": (error|warning):" | head -10 | sed 's/"/\\"/g' | sed "s/'/\\'/g" | awk '{gsub(/\\/,"\\\\"); printf "\"%s\",", $0}' | sed 's/,$//')
if [ -n "$MSG_LIST" ]; then
    MESSAGES="[$MSG_LIST]"
fi

# Write results JSON
cat > "$RESULTS_FILE" << EOF
{
  "success": $SUCCESS,
  "errors": $ERRORS,
  "warnings": $WARNINGS,
  "messages": $MESSAGES
}
EOF

echo "Build results written to $RESULTS_FILE"
echo "  Success: $SUCCESS, Errors: $ERRORS, Warnings: $WARNINGS"

# Exit with appropriate code
if [ "$SUCCESS" = "false" ]; then
    exit 1
fi
exit 0
