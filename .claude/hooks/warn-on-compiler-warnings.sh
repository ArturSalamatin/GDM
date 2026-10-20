#!/bin/bash
# PostToolUse hook: alert Claude when cmake --build produces compiler warnings.
# Reads JSON from stdin, checks tool_response for MSVC warning patterns.

# JSON разбирается python-ом, а не jq: jq в этой системе не установлен.
# Вывод команды лежит в tool_response (у Bash – объект со stdout/stderr),
# поля tool_output нет. Из-за обеих ошибок хук молча выходил с кодом 0.
INPUT=$(cat)
OUTPUT=$(printf '%s' "$INPUT" | python -c "
import sys, json
try:
    r = json.loads(sys.stdin.buffer.read().decode('utf-8', 'replace')).get('tool_response', '')
    if isinstance(r, dict):
        r = '\n'.join(v for v in r.values() if isinstance(v, str))
    sys.stdout.buffer.write(str(r).encode('utf-8', 'replace'))
except Exception:
    pass
" 2>/dev/null)

if [ -z "$OUTPUT" ]; then
  exit 0
fi

# MSBuild повторяет предупреждения в итоговой сводке – считаем уникальные строки.
WARNINGS=$(echo "$OUTPUT" | grep -E 'warning (C|LNK)[0-9]' | sort -u | wc -l)

if [ "$WARNINGS" -gt 0 ]; then
  echo "ВНИМАНИЕ: сборка содержит $WARNINGS предупреждений компилятора/линкера (warning C*/LNK*). Исправь их – warnings = errors." >&2
  exit 2
fi

exit 0
