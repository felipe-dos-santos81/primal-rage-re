"""Cluster the PRAGE.EXE call graph and label the clusters by referenced strings.

Reads port/decomp/prage.calls.csv, prage.functions.csv and prage.strings.csv and
writes port/decomp/prage.clusters.txt. Labelling uses plain label propagation
(no third-party deps).

usage: callgraph.py port/decomp
"""
import csv
import os
import re
import sys
from collections import Counter, defaultdict

base = sys.argv[1] if len(sys.argv) > 1 else 'port/decomp'


def load_edges(path):
    g = defaultdict(set)
    for r in csv.DictReader(open(path)):
        a, b = r['caller'], r['callee']
        if a.startswith('.image') or b.startswith('.image'):
            continue
        g[a].add(b)
        g[b].add(a)
    return g


def addr_off(a):
    m = re.search(r'([0-9a-fA-F]{6,8})$', a)
    return int(m.group(1), 16) if m else None


def main():
    g = load_edges(os.path.join(base, 'prage.calls.csv'))
    # label propagation
    label = {n: n for n in g}
    for it in range(20):
        changed = 0
        for n in sorted(g):
            counts = Counter(label[m] for m in g[n])
            if not counts:
                continue
            best = max(counts.items(), key=lambda kv: (kv[1], kv[0]))[0]
            if label[n] != best:
                label[n] = best
                changed += 1
        if changed == 0:
            break
    clusters = defaultdict(list)
    for n in g:
        clusters[label[n]].append(n)

    # function extents + names
    extent = {}
    for r in csv.DictReader(open(os.path.join(base, 'prage.functions.csv'))):
        try:
            extent[r['entry']] = (int(r['entry'], 16), int(r['size']), r['name'])
        except Exception:
            pass

    # string xrefs -> function -> cluster
    cl_strings = defaultdict(list)
    for r in csv.DictReader(open(os.path.join(base, 'prage.strings.csv'))):
        s = r['string']
        if len(s) < 6 or s.startswith(' '):
            continue
        for xa in r['xref_from'].split():
            try:
                x = int(xa, 16)
            except Exception:
                continue
            for ep, (st, size, name) in extent.items():
                if st <= x < st + size:
                    cl_strings[label[ep]].append(s)
                    break

    out = []
    for root, members in clusters.items():
        if len(members) < 3:
            continue
        offs = [addr_off(m) for m in members if addr_off(m)]
        if not offs:
            continue
        lo, hi = min(offs), max(offs)
        degree = {m: len(g[m]) for m in members}
        top = sorted(members, key=lambda m: -degree[m])[:6]
        names = [extent.get(m, (0, 0, m))[2] for m in top]
        strs = [s for s, _ in Counter(cl_strings[root]).most_common(8)]
        out.append((len(members), lo, hi, names, strs))
    out.sort(reverse=True)
    with open(os.path.join(base, 'prage.clusters.txt'), 'w') as w:
        w.write(f'{len(out)} clusters (size>=3)\n')
        for n, lo, hi, names, strs in out:
            w.write(f'\n== cluster size={n}  {lo:#07x}-{hi:#07x}\n')
            w.write('   top: ' + ' '.join(names) + '\n')
            if strs:
                w.write('   strings: ' + ' | '.join(s[:48] for s in strs) + '\n')
    print(f'{len(out)} clusters -> {base}/prage.clusters.txt')


if __name__ == '__main__':
    main()
