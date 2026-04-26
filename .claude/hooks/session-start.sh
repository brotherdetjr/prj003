#!/bin/bash
python3 - <<'EOF'
import subprocess, json

ctx = (
    "SESSION START — follow CLAUDE.md before doing any work:\n"
    "1. Run: git branch --show-current\n"
    "2. If the branch matches the task, continue on it.\n"
    "3. If unsure whether it matches, ask the user.\n"
    "4. If branch is main or clearly unrelated, suggest creating a new feature branch first.\n"
    "5. Check for merged undeleted branches and handle per CLAUDE.md."
)

try:
    result = subprocess.run(
        ["git", "branch", "--merged", "origin/main"],
        capture_output=True, text=True
    )
    merged = [
        b.strip() for b in result.stdout.splitlines()
        if b.strip() and b.strip() not in ("main", "master") and not b.startswith("*")
    ]
except Exception:
    merged = []

if merged:
    ctx += "\n\nMERGED UNDELETED BRANCHES: " + ", ".join(merged)

print(json.dumps({"hookSpecificOutput": {"hookEventName": "SessionStart", "additionalContext": ctx}}))
EOF
