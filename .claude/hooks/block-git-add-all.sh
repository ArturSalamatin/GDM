#!/bin/bash
# PreToolUse hook: block "git add ." and "git add -A"
# Reads JSON from stdin, checks tool_input.command for forbidden patterns.

INPUT=$(cat)
COMMAND=$(echo "$INPUT" | jq -r '.tool_input.command // empty')

if [ -z "$COMMAND" ]; then
  exit 0
fi

if echo "$COMMAND" | grep -qE '(^|\s|&&|\|\||;)git\s+add\s+(-A|\.)\s*($|;|&&|\|\|)'; then
  echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"git add . / git add -A запрещены. Указывай файлы по имени."}}'
  exit 0
fi

exit 0
