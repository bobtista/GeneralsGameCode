#!/bin/bash
# SemanticScriptConverter Test Suite
# Run this script to verify roundtrip conversion stability
# Usage: ./run_tests.sh [path_to_converter.exe]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Find converter executable
if [ -n "$1" ]; then
    CONVERTER="$1"
elif [ -f "$PROJECT_ROOT/build/win32-debug/GeneralsMD/Debug/semanticscriptconverter.exe" ]; then
    CONVERTER="$PROJECT_ROOT/build/win32-debug/GeneralsMD/Debug/semanticscriptconverter.exe"
elif [ -f "$PROJECT_ROOT/build/win32/GeneralsMD/Release/semanticscriptconverter.exe" ]; then
    CONVERTER="$PROJECT_ROOT/build/win32/GeneralsMD/Release/semanticscriptconverter.exe"
else
    echo "ERROR: Cannot find semanticscriptconverter.exe"
    echo "Usage: $0 [path_to_converter.exe]"
    exit 1
fi

echo "=============================================="
echo "SemanticScriptConverter Test Suite"
echo "=============================================="
echo "Converter: $CONVERTER"
echo "Project root: $PROJECT_ROOT"
echo ""

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
FAILED_TESTS=""

# Temp directory for test artifacts
TEST_TEMP="$PROJECT_ROOT/test_temp"
mkdir -p "$TEST_TEMP"

# Cleanup function
cleanup() {
    rm -rf "$TEST_TEMP"
}
trap cleanup EXIT

# Test function: Verify SCB roundtrip (SCB -> JSON -> SCB -> verify)
test_scb_verify() {
    local scb_file="$1"
    local test_name="$(basename "$scb_file")"

    echo -n "  Testing SCB verify: $test_name ... "
    TESTS_RUN=$((TESTS_RUN + 1))

    if "$CONVERTER" --verify "$scb_file" > "$TEST_TEMP/output.txt" 2>&1; then
        echo "PASS"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo "FAIL"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - $test_name (SCB verify)"
        echo "    Output:"
        tail -20 "$TEST_TEMP/output.txt" | sed 's/^/    /'
        return 1
    fi
}

# Test function: Verify JSON roundtrip (JSON -> SCB -> JSON -> compare)
test_json_roundtrip() {
    local json_file="$1"
    local test_name="$(basename "$json_file")"

    echo -n "  Testing JSON roundtrip: $test_name ... "
    TESTS_RUN=$((TESTS_RUN + 1))

    local scb_temp="$TEST_TEMP/roundtrip.scb"
    local json_temp="$TEST_TEMP/roundtrip.json"

    # JSON -> SCB
    if ! "$CONVERTER" "$json_file" "$scb_temp" > "$TEST_TEMP/output.txt" 2>&1; then
        echo "FAIL (JSON->SCB conversion)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - $test_name (JSON->SCB failed)"
        tail -10 "$TEST_TEMP/output.txt" | sed 's/^/    /'
        return 1
    fi

    # Verify the SCB is valid
    if ! "$CONVERTER" --validate "$scb_temp" > "$TEST_TEMP/output.txt" 2>&1; then
        echo "FAIL (SCB validation)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - $test_name (SCB validation failed)"
        tail -10 "$TEST_TEMP/output.txt" | sed 's/^/    /'
        return 1
    fi

    # SCB -> JSON
    if ! "$CONVERTER" "$scb_temp" "$json_temp" > "$TEST_TEMP/output.txt" 2>&1; then
        echo "FAIL (SCB->JSON conversion)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - $test_name (SCB->JSON failed)"
        tail -10 "$TEST_TEMP/output.txt" | sed 's/^/    /'
        return 1
    fi

    # Verify roundtrip on the generated SCB
    if ! "$CONVERTER" --verify "$scb_temp" > "$TEST_TEMP/output.txt" 2>&1; then
        echo "FAIL (roundtrip verify)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - $test_name (roundtrip verify failed)"
        tail -20 "$TEST_TEMP/output.txt" | sed 's/^/    /'
        return 1
    fi

    echo "PASS"
    TESTS_PASSED=$((TESTS_PASSED + 1))
    return 0
}

# Test function: Validate file
test_validate() {
    local file="$1"
    local test_name="$(basename "$file")"

    echo -n "  Testing validation: $test_name ... "
    TESTS_RUN=$((TESTS_RUN + 1))

    if "$CONVERTER" --validate "$file" > "$TEST_TEMP/output.txt" 2>&1; then
        echo "PASS"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo "FAIL"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - $test_name (validation)"
        tail -10 "$TEST_TEMP/output.txt" | sed 's/^/    /'
        return 1
    fi
}

# Test function: Check that converter produces non-zero output
test_conversion_produces_output() {
    local input="$1"
    local output="$2"
    local test_name="$(basename "$input") -> $(basename "$output")"

    echo -n "  Testing conversion: $test_name ... "
    TESTS_RUN=$((TESTS_RUN + 1))

    rm -f "$output"

    if ! "$CONVERTER" "$input" "$output" > "$TEST_TEMP/output.txt" 2>&1; then
        echo "FAIL (conversion error)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - $test_name (conversion error)"
        tail -10 "$TEST_TEMP/output.txt" | sed 's/^/    /'
        return 1
    fi

    if [ ! -f "$output" ]; then
        echo "FAIL (no output file)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - $test_name (no output file)"
        return 1
    fi

    local size=$(stat -c%s "$output" 2>/dev/null || stat -f%z "$output" 2>/dev/null)
    if [ "$size" -eq 0 ]; then
        echo "FAIL (0-byte output)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - $test_name (0-byte output)"
        return 1
    fi

    echo "PASS ($size bytes)"
    TESTS_PASSED=$((TESTS_PASSED + 1))
    return 0
}

echo "----------------------------------------------"
echo "Phase 1: Validation Tests"
echo "----------------------------------------------"

# Find and test all JSON files in project root
for json_file in "$PROJECT_ROOT"/*.json; do
    if [ -f "$json_file" ]; then
        # Skip non-script JSON files
        if grep -q '"players"' "$json_file" 2>/dev/null; then
            test_validate "$json_file" || true
        fi
    fi
done

# Find and test all SCB files in project root
for scb_file in "$PROJECT_ROOT"/*.scb; do
    if [ -f "$scb_file" ]; then
        test_validate "$scb_file" || true
    fi
done

echo ""
echo "----------------------------------------------"
echo "Phase 2: JSON Roundtrip Tests"
echo "----------------------------------------------"

for json_file in "$PROJECT_ROOT"/*.json; do
    if [ -f "$json_file" ]; then
        # Only test script JSON files (have "players" array)
        if grep -q '"players"' "$json_file" 2>/dev/null; then
            test_json_roundtrip "$json_file" || true
        fi
    fi
done

echo ""
echo "----------------------------------------------"
echo "Phase 3: SCB Verify Tests"
echo "----------------------------------------------"

for scb_file in "$PROJECT_ROOT"/*.scb; do
    if [ -f "$scb_file" ]; then
        test_scb_verify "$scb_file" || true
    fi
done

echo ""
echo "----------------------------------------------"
echo "Phase 4: Conversion Output Tests"
echo "----------------------------------------------"

# Test that JSON->SCB produces non-zero output
for json_file in "$PROJECT_ROOT"/*.json; do
    if [ -f "$json_file" ]; then
        if grep -q '"players"' "$json_file" 2>/dev/null; then
            base_name=$(basename "$json_file" .json)
            test_conversion_produces_output "$json_file" "$TEST_TEMP/${base_name}_test.scb" || true
        fi
    fi
done

# Test that SCB->JSON produces non-zero output
for scb_file in "$PROJECT_ROOT"/*.scb; do
    if [ -f "$scb_file" ]; then
        base_name=$(basename "$scb_file" .scb)
        test_conversion_produces_output "$scb_file" "$TEST_TEMP/${base_name}_test.json" || true
    fi
done

echo ""
echo "=============================================="
echo "Test Results"
echo "=============================================="
echo "Total tests: $TESTS_RUN"
echo "Passed: $TESTS_PASSED"
echo "Failed: $TESTS_FAILED"

if [ $TESTS_FAILED -gt 0 ]; then
    echo ""
    echo "Failed tests:"
    echo -e "$FAILED_TESTS"
    echo ""
    echo "RESULT: FAILURE"
    exit 1
else
    echo ""
    echo "RESULT: SUCCESS - All tests passed!"
    exit 0
fi
