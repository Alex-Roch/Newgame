#!/usr/bin/env python3
"""Summarise bench.sh outputs (meminfo + com_speeds) and, optionally, HUNK_DEBUG hunklog output.

  tools/desktop/summarize.py <workdir>/bench/*.txt          -> table of hunk/zone/frame times
  tools/desktop/summarize.py --hunklog <home>/baseoa/console.log  -> hunk allocations by label
"""
import collections
import re
import statistics
import sys


def summarise(paths):
    print(f"{'config':22s} {'hunkMB':>7s} {'lowPerm':>8s} {'hiPerm':>7s} {'zoneMB':>6s} {'n':>4s} "
          f"{'all':>6s} {'sv':>5s} {'cl':>5s} {'gm':>5s} {'rf':>5s} {'bk':>5s}")
    for p in paths:
        t = open(p, errors="replace").read()
        name = p.split("/")[-1].removesuffix(".txt")

        def g(pat):
            m = re.search(pat, t)
            return int(m.group(1)) / 1048576 if m else float("nan")

        hunk = g(r"(\d+) total hunk in use")
        lp = g(r"(\d+) low permanent")
        hp = g(r"(\d+) high permanent")
        zone = g(r"(\d+) bytes in \d+ zone blocks")
        fr = [tuple(map(int, f)) for f in re.findall(
            r"frame:\d+ all:\s*(\d+) sv:\s*(\d+) ev:\s*(\d+) cl:\s*(\d+) gm:\s*(\d+) rf:\s*(\d+) bk:\s*(\d+)", t)]
        fr = fr[30:] if len(fr) > 60 else fr  # drop warm-up frames
        if fr:
            avg = [statistics.mean(x[i] for x in fr) for i in range(7)]
            print(f"{name:22s} {hunk:7.1f} {lp:8.1f} {hp:7.1f} {zone:6.1f} {len(fr):4d} "
                  f"{avg[0]:6.2f} {avg[1]:5.2f} {avg[3]:5.2f} {avg[4]:5.2f} {avg[5]:5.2f} {avg[6]:5.2f}")
        else:
            print(f"{name:22s} {hunk:7.1f} {lp:8.1f} {hp:7.1f} {zone:6.1f}    0  (no com_speeds frames)")


def hunklog(path):
    t = open(path, errors="replace").read()
    rows = re.findall(r"size =\s*(\d+): (\S+), line: (\d+) \((.*?)\)", t)
    agg = collections.Counter()
    files = collections.Counter()
    for s, f, ln, label in rows:
        agg[(label[:34], f.split("/")[-1] + ":" + ln)] += int(s)
        files[f.split("/")[-1]] += int(s)
    print(f"logged total {sum(agg.values()) / 1048576:.1f} MB in {len(rows)} allocations")
    for (label, where), s in agg.most_common(26):
        print(f"  {s / 1048576:7.2f} MB  {label:34s} {where}")
    print("by file:")
    for f, s in files.most_common(14):
        print(f"  {s / 1048576:7.2f} MB  {f}")


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--hunklog":
        hunklog(args[1])
    else:
        summarise(args)
