#!/usr/bin/env python3
"""PreToolUse(Bash) hook: gate `git push` to the DEVELOPMENT branch (safian/main) only.

The push is allowed when the last green verification (.claude/hooks/verify-green.sh, which writes the verified
HEAD sha to .git/kick-verified-head) covers the commits being pushed: marker == HEAD, or everything since the marker
touches only docs/markdown/.claude files. Pushes to any other branch are never gated. Force pushes are NOT forbidden.
Exit code 2 = block (stderr goes back to the agent).
"""
import json, re, subprocess, sys

DEV = "safian/main"
try:
    data = json.load(sys.stdin)
except Exception:
    sys.exit(0)
if data.get("tool_name") != "Bash":
    sys.exit(0)
cmd = (data.get("tool_input") or {}).get("command") or ""
cmd = re.sub(r"<<-?\s*(['\"]?)(\w+)\1[^\n]*\n.*?\n\s*\2\s*(?=\n|$)", "<<HEREDOC", cmd, flags=re.S)

def sh(*a):
    try:
        return subprocess.run(a, capture_output=True, text=True, timeout=10).stdout.strip()
    except Exception:
        return ""

pushes = [m for m in re.finditer(r"(?:^|[;&|(`]|\$\(|\n)\s*git\s+(?:-C\s+\S+\s+)?push\b([^;&|\n]*)", cmd)]
if not pushes:
    sys.exit(0)

cur = sh("git", "rev-parse", "--abbrev-ref", "HEAD")
gated = False
for m in pushes:
    args = [a for a in m.group(1).split() if not a.startswith("-")]
    refspecs = args[1:] if len(args) >= 1 else []          # args[0] is the remote
    if not refspecs or refspecs == ["HEAD"]:
        gated |= (cur == DEV)
    else:
        gated |= any(DEV in r.split(":")[-1] or DEV in r for r in refspecs)
if not gated:
    sys.exit(0)

head = sh("git", "rev-parse", "HEAD")
marker = sh("cat", ".git/kick-verified-head")
ok = False
if marker:
    if marker == head:
        ok = True
    else:
        files = sh("git", "diff", "--name-only", f"{marker}..{head}").split("\n")
        files = [f for f in files if f]
        if files and all(f.startswith(("docs/", ".claude/")) or f.endswith(".md") for f in files):
            ok = True
if ok:
    sys.exit(0)
sys.stderr.write(
    "PUSH-KAPU: a fejlesztoi agra (safian/main) pusholas elott zold build+teszt kell az aktualis HEAD-re. "
    "Futtasd HATTERBEN (vagy workerrel): bash .claude/hooks/verify-green.sh  -- sikeres futas utan a push engedett. "
    "(Csak docs/markdown/.claude valtozas a legutolso zold futas ota ujraellenorzes nelkul mehet.) Mas agra a kapu nem vonatkozik; force-push nem tiltott.\n")
sys.exit(2)
