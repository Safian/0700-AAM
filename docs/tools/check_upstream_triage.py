#!/usr/bin/env python3
"""Verify docs/UPSTREAM_TRIAGE.md against the git history.

For every commit-hash reference in the document (backticked, 7-40 hex digits) it checks:
  1. the hash resolves to a commit (git cat-file -e),
  2. the commit is inside <base>..<head> (default origin/main..HEAD), or inside
     <base>..<ref> for any --also ref (default: fork/feat/mobile-slideout-ui),
     except the base commit itself, which is allowed in the metrics block.

For every ticket section ("#### TICKET-ID: title"):
  3. every path listed under "Érintett fájlok" appears in the diff of at least one of
     the ticket's commits (git show --first-parent -m --name-only),
  4. the "Becsült méret" line equals the real sum of the ticket's non-merge commit
     diffs (unique files, +insertions / -deletions from git show --numstat),
  5. no commit is listed in two tickets.
For the summary table:
  6. every hash and path (base name) in a ticket row appears in that ticket's commits.
Coverage:
  7. every non-merge commit in <base>..<head> is listed in exactly one ticket
     (merge commits are not tickets; --also commits are optional).
Headline numbers:
  8. tip hash, commit count (total / non-merge / merge), diff shortstat, Proper file count.

Usage: check_upstream_triage.py [--doc FILE] [--base REF] [--head REF] [--also REF]... [-q]
Exit code 0 = clean, 1 = problems found. Python 3 standard library only.
"""
import argparse
import os
import re
import subprocess
import sys


def git(*args, check=False):
    r = subprocess.run(["git", *args], capture_output=True, text=True)
    if check and r.returncode:
        sys.exit("git %s failed: %s" % (" ".join(args), r.stderr.strip()))
    return r


DEFAULT_ALSO = ["fork/feat/mobile-slideout-ui", "fork/fix/project-relative-samples"]
BACKTICK_HASH_RE = re.compile(r"`([0-9a-f]{7,40})`")
TICKET_HEAD_RE = re.compile(r"^####\s+((?:UPSTREAM|FORK)(?:-[A-Z0-9]+)+):")
SHORTSTAT_RE = re.compile(r"(\d+) files? changed(?:, (\d+) insertions?\(\+\))?(?:, (\d+) deletions?\(-\))?")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--doc", default="docs/UPSTREAM_TRIAGE.md")
    ap.add_argument("--base", default="origin/main")
    ap.add_argument("--head", default="HEAD")
    ap.add_argument("--also", action="append", default=None,
                    help="extra ref whose <base>..<ref> commits may be referenced (default: %s)" % ", ".join(DEFAULT_ALSO))
    ap.add_argument("-q", action="store_true", help="only print problems and the summary")
    a = ap.parse_args()
    also = a.also if a.also is not None else DEFAULT_ALSO

    top = git("rev-parse", "--show-toplevel", check=True).stdout.strip()
    os.chdir(top)
    text = open(a.doc, encoding="utf-8").read()
    lines = text.splitlines()

    base_full = git("rev-parse", a.base, check=True).stdout.strip()
    head_full = git("rev-parse", a.head, check=True).stdout.strip()
    in_range = set(git("rev-list", "%s..%s" % (a.base, a.head), check=True).stdout.split())
    allowed = set(in_range)
    for ref in also:
        r = git("rev-list", "%s..%s" % (a.base, ref))
        if r.returncode == 0:
            allowed |= set(r.stdout.split())
        elif not a.q:
            print("note: --also ref %s not found, skipped" % ref)
    no_merge_range = set(git("rev-list", "--no-merges", "%s..%s" % (a.base, a.head), check=True).stdout.split())

    resolved = {}

    def resolve(h):
        if h not in resolved:
            r = git("rev-parse", "--verify", "--quiet", h + "^{commit}")
            full = r.stdout.strip() if r.returncode == 0 else None
            if full and git("cat-file", "-e", full).returncode != 0:
                full = None
            resolved[h] = full
        return resolved[h]

    cache_files = {}

    def files_of(full):
        if full not in cache_files:
            r = git("show", "--first-parent", "-m", "--name-only", "--format=", full)
            cache_files[full] = set(x for x in r.stdout.splitlines() if x)
        return cache_files[full]

    cache_num = {}

    def numstat_of(full):
        if full not in cache_num:
            r = git("show", "--first-parent", "--numstat", "--format=", full)
            rows = []
            for ln in r.stdout.splitlines():
                p = ln.split("\t")
                if len(p) >= 3:
                    rows.append((p[2], int(p[0]) if p[0] != "-" else 0, int(p[1]) if p[1] != "-" else 0))
            cache_num[full] = rows
        return cache_num[full]

    def is_merge(full):
        return len(git("rev-list", "--parents", "-n1", full).stdout.split()) > 2

    problems = []

    # 1-2: hash references
    refs = {}
    for i, ln in enumerate(lines, 1):
        for m in BACKTICK_HASH_RE.finditer(ln):
            h = m.group(1)
            if re.search(r"\d", h):
                refs.setdefault(h, []).append(i)
    missing, outside = [], []
    for h, lns in sorted(refs.items(), key=lambda kv: kv[1][0]):
        full = resolve(h)
        if full is None:
            missing.append((h, lns))
        elif full not in allowed and full != base_full:
            outside.append((h, lns))
    for h, lns in missing:
        problems.append("MISSING hash %s (lines %s)" % (h, ",".join(map(str, lns[:5]))))
    for h, lns in outside:
        problems.append("OUT-OF-RANGE hash %s not in %s..%s or %s (lines %s)"
                        % (h, a.base, a.head, "/".join(also), ",".join(map(str, lns[:5]))))

    # collect tickets
    heads = [(i, TICKET_HEAD_RE.match(l).group(1)) for i, l in enumerate(lines) if TICKET_HEAD_RE.match(l)]
    tickets = []
    for idx, (i, tid) in enumerate(heads):
        end = heads[idx + 1][0] if idx + 1 < len(heads) else len(lines)
        for j in range(i + 1, end):
            if lines[j].startswith("## ") or lines[j].startswith("### "):
                end = j
                break
        tickets.append((tid, i, end))

    owner = {}  # full hash -> ticket id
    ticket_commits = {}
    path_errors = size_errors = 0
    for tid, s, e in tickets:
        block = lines[s:e]
        hashes, paths = [], []
        claimed = None
        mode = None
        for ln in block:
            if ln.startswith("- **Tartalmazott commitok"):
                mode = "c"
                continue
            if ln.startswith("- **Érintett fájlok"):
                mode = "f"
                continue
            if ln.startswith("- **"):
                mode = None
                m = re.match(r"- \*\*Becsült méret:\*\* (\d+) fájl, \+(\d+) / -(\d+)", ln)
                if m:
                    claimed = tuple(int(x) for x in m.groups())
                continue
            if mode == "c":
                m = re.match(r"^\s+- `([0-9a-f]{7,40})`", ln)
                if m:
                    hashes.append(m.group(1))
            elif mode == "f":
                m = re.match(r"^\s+- `([^`]+)`", ln)
                if m:
                    paths.append(m.group(1))
        fulls = []
        for h in hashes:
            f = resolve(h)
            if f:
                fulls.append(f)
                if f in owner and owner[f] != tid:
                    problems.append("DUPLICATE commit %s listed in %s and %s" % (h, owner[f], tid))
                owner.setdefault(f, tid)
        ticket_commits[tid] = fulls
        union = set()
        for f in fulls:
            union |= files_of(f)
        for p in paths:
            if p not in union:
                problems.append("PATH %s: '%s' not in the diff of any listed commit" % (tid, p))
                path_errors += 1
        if claimed is not None:
            files, ins, dele = set(), 0, 0
            for f in fulls:
                if is_merge(f):
                    continue
                for path, ai, de in numstat_of(f):
                    files.add(path)
                    ins += ai
                    dele += de
            real = (len(files), ins, dele)
            if real != claimed:
                problems.append("SIZE %s: claimed %d files +%d/-%d, real %d files +%d/-%d"
                                % ((tid,) + claimed + real))
                size_errors += 1
        if not a.q:
            print("ticket %-18s commits=%-3d paths=%-3d" % (tid, len(hashes), len(paths)))

    # 6: summary table rows
    table_errors = 0
    for i, ln in enumerate(lines, 1):
        m = re.match(r"^\| \*\*((?:UPSTREAM|FORK)(?:-[A-Z0-9]+)+)\*\* \|", ln)
        if not m:
            continue
        tid = m.group(1)
        if tid not in ticket_commits:
            problems.append("TABLE row %s (line %d) has no detail section" % (tid, i))
            table_errors += 1
            continue
        fulls = ticket_commits[tid]
        union = set()
        for f in fulls:
            union |= files_of(f)
        base_names = set(p.split("/")[-1] for p in union)
        cells = ln.split(" | ")
        for h in BACKTICK_HASH_RE.findall(ln):
            f = resolve(h)
            if f and f not in fulls:
                problems.append("TABLE %s (line %d): hash %s is not a commit of the ticket" % (tid, i, h))
                table_errors += 1
        # path cell = 6th column (index 5)
        if len(cells) > 5:
            for name in re.findall(r"`([^`]+)`", cells[5]):
                if name not in base_names:
                    problems.append("TABLE %s (line %d): path '%s' not in the ticket's diffs" % (tid, i, name))
                    table_errors += 1

    # 7: coverage
    uncovered = sorted(c for c in no_merge_range if c not in owner)
    for c in uncovered:
        subj = git("log", "-1", "--format=%s", c).stdout.strip()
        problems.append("UNCOVERED non-merge commit %s not in any ticket: %s" % (c[:8], subj[:80]))

    # 8: headline numbers
    total = len(in_range)
    nm = len(no_merge_range)
    m = re.search(r"Fork csúcs[^\n]*?`([0-9a-f]{40})`", text)
    if not m:
        problems.append("HEADLINE: tip hash line not found")
    elif m.group(1) != head_full:
        problems.append("HEADLINE tip hash %s != %s (%s)" % (m.group(1)[:8], head_full[:8], a.head))
    m = re.search(r"Upstream bázis[^\n]*?`([0-9a-f]{40})`", text)
    if not m:
        problems.append("HEADLINE: base hash line not found")
    elif m.group(1) != base_full:
        problems.append("HEADLINE base hash %s != %s (%s)" % (m.group(1)[:8], base_full[:8], a.base))
    m = re.search(r"\*\*(\d+) commit\*\*[^\n]*?: (\d+) nem-merge és (\d+) merge", text)
    if not m:
        problems.append("HEADLINE: commit count line not found")
    else:
        got = tuple(int(x) for x in m.groups())
        if got != (total, nm, total - nm):
            problems.append("HEADLINE commit count %s != actual (total %d, non-merge %d, merge %d)"
                            % (got, total, nm, total - nm))
    sm = SHORTSTAT_RE.search(git("diff", "--shortstat", "%s...%s" % (a.base, a.head)).stdout)
    m = re.search(r"\*\*(\d+) fájl\*\*, `\+(\d+) / -(\d+)` sor", text)
    if not m:
        problems.append("HEADLINE: diff size line not found")
    elif sm and tuple(int(x or 0) for x in sm.groups()) != tuple(int(x) for x in m.groups()):
        problems.append("HEADLINE diff %s != actual %s" % (m.groups(), tuple(x or "0" for x in sm.groups())))
    pc = len(git("ls-tree", "-r", "--name-only", a.head, "Kick/assets/Proper/").stdout.split("\n")) - 1
    m = re.search(r"\*\*(\d+) fájl\*\* a `Kick/assets/Proper/`", text)
    if not m:
        problems.append("HEADLINE: Proper file count line not found")
    elif int(m.group(1)) != pc:
        problems.append("HEADLINE Proper file count %s != actual %d" % (m.group(1), pc))

    print("hashes referenced: %d  missing: %d  out-of-range: %d  tickets: %d  path-errors: %d  size-errors: %d  "
          "table-errors: %d  uncovered non-merge commits: %d  (range %s..%s: %d commits, %d non-merge)"
          % (len(refs), len(missing), len(outside), len(tickets), path_errors, size_errors, table_errors,
             len(uncovered), a.base, a.head, total, nm))
    if problems:
        print("\nPROBLEMS (%d):" % len(problems))
        for p in problems:
            print("  " + p)
        return 1
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
