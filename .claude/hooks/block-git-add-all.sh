#!/bin/bash
# PreToolUse hook: block "git add ." and "git add -A"
# Reads JSON from stdin, checks tool_input.command for forbidden patterns.

# JSON разбирается python-ом, а не jq: jq в этой системе не установлен,
# из-за чего хук молча не срабатывал – падал с "jq: command not found",
# выходил с кодом 0, и запрещённая команда проходила дальше.
INPUT=$(cat)
COMMAND=$(printf '%s' "$INPUT" | python -c "
import sys, json
try:
    print(json.load(sys.stdin).get('tool_input', {}).get('command', ''))
except Exception:
    pass
" 2>/dev/null)

if [ -z "$COMMAND" ]; then
  exit 0
fi

# `-C <путь>` / `-c <ключ=значение>` перед add допускаются: worktree-процесс (/implement, /merge)
# работает через `git -C <каталог>`. `--all` – длинная форма `-A`.
if echo "$COMMAND" | grep -qE '(^|\s|&&|\|\||;)git(\s+-[Cc]\s+\S+)*\s+add\s+(-A|--all|\.)\s*($|;|&&|\|\|)'; then
  echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"git add . / git add -A запрещены. Указывай файлы по имени."}}'
  exit 0
fi

exit 0
