#!/bin/bash
# Test runner for formatter
# Usage: ./run_tests.sh [path/to/formatter]

set -e

FORMATTER="${1:-../formatter}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0
TOTAL=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Run a single test
# Args: test_name input expected [options...]
run_test() {
    local name="$1"
    local input="$2"
    local expected="$3"
    shift 3
    local opts=("$@")
    
    TOTAL=$((TOTAL + 1))
    
    # Run formatter and capture output
    local actual
    actual=$(echo -n "$input" | "$FORMATTER" "${opts[@]}" 2>&1) || true
    
    if [[ "$actual" == "$expected" ]]; then
        echo -e "${GREEN}PASS${NC}: $name"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC}: $name"
        echo "  Input:    $(echo -n "$input" | cat -v)"
        echo "  Expected: $(echo -n "$expected" | cat -v)"
        echo "  Actual:   $(echo -n "$actual" | cat -v)"
        FAIL=$((FAIL + 1))
    fi
}

# Run a test expecting specific ANSI output (with escapes visible)
run_ansi_test() {
    local name="$1"
    local input="$2"
    local expected="$3"
    shift 3
    local opts=("$@")
    
    TOTAL=$((TOTAL + 1))
    
    # Run formatter and capture raw output with cat -v to see escapes
    local actual
    actual=$(echo -n "$input" | "$FORMATTER" "${opts[@]}" 2>&1 | cat -v) || true
    
    if [[ "$actual" == "$expected" ]]; then
        echo -e "${GREEN}PASS${NC}: $name"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC}: $name"
        echo "  Input:    $input"
        echo "  Expected: $expected"
        echo "  Actual:   $actual"
        FAIL=$((FAIL + 1))
    fi
}

echo "========================================"
echo "Formatter Test Suite"
echo "Using: $FORMATTER"
echo "========================================"
echo

# Check formatter exists
if [[ ! -x "$FORMATTER" ]]; then
    echo -e "${RED}ERROR${NC}: Formatter not found or not executable: $FORMATTER"
    echo "Build with: make build"
    exit 1
fi

# =============================================================================
echo "--- Strip Mode Tests (-s) ---"
# =============================================================================

run_test "strip: plain text" \
    "hello world" \
    "hello world" \
    -s

run_test "strip: simple format" \
    "{r--red--}" \
    "red" \
    -s

run_test "strip: nested format" \
    "{r--outer {b--inner--} outer--}" \
    "outer inner outer" \
    -s

run_test "strip: multiple formats" \
    "{*--bold--} {/--italic--}" \
    "bold italic" \
    -s

run_test "strip: unbalanced open" \
    "{r--unclosed" \
    "unclosed" \
    -s

run_test "strip: format with colors" \
    "{RB--bright red on blue--}" \
    "bright red on blue" \
    -s

# =============================================================================
echo
echo "--- Escape Sequence Tests (-e) ---"
# =============================================================================

run_test "escape: backslash" \
    "\\\\" \
    "\\" \
    -se

run_test "escape: newline" \
    "a\\nb" \
    $'a\nb' \
    -se

run_test "escape: tab" \
    "a\\tb" \
    $'a\tb' \
    -se

run_test "escape: carriage return" \
    "XXX\\rYYY" \
    $'XXX\rYYY' \
    -se

run_test "escape: trim whitespace" \
    "hello\\#     world" \
    "helloworld" \
    -se

run_test "escape: trim multiple spaces" \
    "a\\#   \\#   b" \
    "ab" \
    -se

run_test "escape: trim preserves newline after non-ws" \
    "trim\\#   X" \
    "trimX" \
    -se

run_test "escape: invalid escape passed through" \
    "\\q" \
    "\\q" \
    -se

# =============================================================================
echo
echo "--- Format Parsing Tests ---"
# =============================================================================

run_test "parse: empty format" \
    "{--text--}" \
    "text" \
    -s

run_test "parse: single color fg" \
    "{r--red--}" \
    "red" \
    -s

run_test "parse: fg and bg" \
    "{rb--red on blue--}" \
    "red on blue" \
    -s

run_test "parse: current color" \
    "{;b--current fg, blue bg--}" \
    "current fg, blue bg" \
    -s

run_test "parse: default color" \
    "{dd--default--}" \
    "default" \
    -s

run_test "parse: bright colors" \
    "{RB--bright red on bright blue--}" \
    "bright red on bright blue" \
    -s

run_test "parse: all styles" \
    "{*--b--}{/--i--}{_--u--}{~--s--}{.--d--}{%--r--}" \
    "biusdr" \
    -s

run_test "parse: reset" \
    "{0--reset--}" \
    "reset" \
    -s

run_test "parse: too many colors fails" \
    "{rgb--invalid--}" \
    "{rgb--invalid--}" \
    -s

run_test "parse: duplicate style fails" \
    "{**--invalid--}" \
    "{**--invalid--}" \
    -s

# =============================================================================
echo
echo "--- Nesting Tests ---"
# =============================================================================

run_test "nest: simple" \
    "{r--{b--blue--}--}" \
    "blue" \
    -s

run_test "nest: deep" \
    "{*--{/--{_--text--}--}--}" \
    "text" \
    -s

run_test "nest: mixed text" \
    "{r--red {b--blue--} red--}" \
    "red blue red" \
    -s

run_test "nest: style toggle" \
    "{*--bold {*--not bold--} bold--}" \
    "bold not bold bold" \
    -s

# =============================================================================
echo
echo "--- Edge Cases ---"
# =============================================================================

run_test "edge: empty input" \
    "" \
    "" \
    -s

run_test "edge: just braces" \
    "{}" \
    "{}" \
    -s

run_test "edge: incomplete open" \
    "{r-text" \
    "{r-text" \
    -s

run_test "edge: incomplete close" \
    "text-}" \
    "text-}" \
    -s

run_test "edge: trippy minimal" \
    "{--}" \
    "}" \
    -s

run_test "edge: unmatched close" \
    "text--}" \
    "text--}" \
    -s

run_test "edge: utf8" \
    "{r--你好世界--}" \
    "你好世界" \
    -s

run_test "edge: literal dashes" \
    "a--b" \
    "a--b" \
    -s

# =============================================================================
echo
echo "========================================"
echo "Results: $PASS passed, $FAIL failed, $TOTAL total"
echo "========================================"

if [[ $FAIL -gt 0 ]]; then
    exit 1
fi
