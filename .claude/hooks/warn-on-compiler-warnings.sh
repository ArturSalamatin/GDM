#!/bin/bash
# PostToolUse hook: alert Claude when cmake --build produces compiler warnings.
# Reads JSON from stdin, checks tool_output for MSVC warning patterns.

INPUT=$(cat)
OUTPUT=$(echo "$INPUT" | jq -r '.tool_output // empty')

if [ -z "$OUTPUT" ]; then
  exit 0
fi

WARNINGS=$(echo "$OUTPUT" | grep -c 'warning C[0-9]')

if [ "$WARNINGS" -gt 0 ]; then
  echo "ВНИМАНИЕ: сборка содержит $WARNINGS предупреждений компилятора (warning C*). Исправь их — warnings = errors." >&2
  exit 2
fi

exit 0
