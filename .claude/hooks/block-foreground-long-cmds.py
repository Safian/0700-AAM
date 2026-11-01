#!/usr/bin/env python3
"""PreToolUse(Bash) hook: block long-running FOREGROUND commands.

Policy (user decision): builds, test runs, smoke runs and wait loops must run with
run_in_background=true or be delegated to a worker (Agent tool). Only short commands
may run in the foreground. Exit code 2 = block, stderr is fed back to the agent.
"""
import json, re, sys

MSG = ("BLOKKOLVA: hosszu elotéri parancs ({why}). A szabaly: build, teszt, smoke-futtatas, "
       "varakozo ciklus csak run_in_background:true-val, vagy delegald workernek (Agent tool). "
       "Elotérben csak rovid parancs (git status, ls, grep, cat, kis olvasas) futhat.")

try:
    data = json.load(sys.stdin)
except Exception:
    sys.exit(0)  # never break the session on a malformed payload

if data.get("tool_name") != "Bash":
    sys.exit(0)
ti = data.get("tool_input") or {}
if ti.get("run_in_background") is True:
    sys.exit(0)

cmd = ti.get("command") or ""
# Ignore heredoc bodies (commit messages, scripts being written) so words inside them do not match.
cmd = re.sub(r"<<-?\s*(['\"]?)(\w+)\1[^\n]*\n.*?\n\s*\2\s*(?=\n|$)", "<<HEREDOC", cmd, flags=re.S)

POS = r"(?:^|[;&|(`]|\$\(|\n)\s*"  # command position
checks = [
    (POS + r"(?:ninja|ctest)\b", "ninja/ctest"),
    (POS + r"cmake\s+(?:--build|--install)\b", "cmake --build"),
    (POS + r"make\b", "make"),
    (POS + r"xvfb-run\b", "xvfb-run smoke-futtatas"),
    (POS + r"(?:bash|sh)\s+\S*verify-green\.sh", "verify-green.sh (hatterben kell)"),
    (POS + r"(?:until|while)\b", "varakozo ciklus"),
    (POS + r"for\b[^\n]*\bg\+\+(?=\s)", "g++ ciklus"),
    (r"\bsleep\s+(?:[1-9]\d+|\d+\.\d+\d)\b", "sleep >= 10 s"),
    (r"\btimeout\s+(?:[1-9]\d{2,})\b", "hosszu timeout"),
]
for pat, why in checks:
    if re.search(pat, cmd):
        sys.stderr.write(MSG.format(why=why) + "\n")
        sys.exit(2)
sys.exit(0)
