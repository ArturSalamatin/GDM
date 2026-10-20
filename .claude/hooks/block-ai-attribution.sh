#!/bin/bash
# PreToolUse hook: блокировать git commit / gh pr с упоминанием ИИ в сообщении.
#
# CLAUDE.md запрещает упоминать Claude в сообщениях коммитов. Основная защита –
# "attribution": {"commit": "", "pr": ""} в ~/.claude/settings.json: она не даёт
# трейлеру появиться вовсе. Этот хук – страховка на случай, если строка попадёт
# в сообщение другим путём (heredoc, -m, шаблон, ручной ввод).
#
# JSON со stdin разбирается python-ом, а не jq: jq в этой системе не установлен
# (из-за чего соседний block-git-add-all.sh молча не срабатывал).

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

# Реагируем только на команды, создающие сообщение: git commit, gh pr create/edit.
# git log --grep=... и прочее чтение пропускаем. `-C <путь>` / `-c <ключ=значение>`
# перед commit допускаются: /implement и /merge коммитят через `git -C <КОРЕНЬ> commit`.
if ! printf '%s' "$COMMAND" | grep -qE '(^|[[:space:];&|])(git([[:space:]]+-[Cc][[:space:]]+[^[:space:]]+)*[[:space:]]+commit|gh[[:space:]]+pr[[:space:]]+(create|edit))'; then
  exit 0
fi

# Запрещённые подписи, регистр не важен:
#   Co-Authored-By с Claude / Anthropic / noreply@anthropic
#   "Generated with ... Claude"
#   эмодзи робота (🤖) из шаблона PR
# `.*`, а не `[^\n]*`: в POSIX ERE `[^\n]` – «не `\` и не `n`», и подпись
# с `noreply@anthropic.com` проходила (буква `n` между двоеточием и именем).
if printf '%s' "$COMMAND" | grep -qiE 'co-authored-by:.*(claude|anthropic)|generated with.*claude|🤖'; then
  echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"В сообщениях коммитов и PR нельзя упоминать ИИ (CLAUDE.md). Убери строку Co-Authored-By / Generated with Claude и повтори."}}'
  exit 0
fi

exit 0
