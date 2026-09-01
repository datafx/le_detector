#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""
extract_oui.py - pull MAC/OUI prefixes out of a firmware binary and diff them
against our own watchlist in src/oui_table.cpp.

Why this is likely to work: nyanBOX's published detectors store OUIs as ASCII
string literals, not packed bytes. flock_detector.cpp has

    const char* mac_prefixes[] = { "58:8e:81", "cc:cc:cc", ... };

and axon_detector.cpp does strncasecmp(addrStr, "00:25:df", 8). If the LE Gear
Detector follows suit, the table is plain text inside the .bin and mode "ascii"
below will find it with no disassembly.

Usage:
    # 1. ASCII scan (try this first)
    python3 extract_oui.py firmware.bin --ieee master_oui.csv

    # 2. If ASCII finds nothing, try the packed-byte heuristic
    python3 extract_oui.py firmware.bin --ieee master_oui.csv --mode packed

    # 3. Diff against our table
    python3 extract_oui.py firmware.bin --ieee master_oui.csv \
        --compare ../src/oui_table.cpp

Get the IEEE CSV from:
    https://github.com/Ringmast4r/OUI-Master-Database  (LISTS/master_oui.csv)
"""

import argparse
import csv
import re
import sys
from collections import OrderedDict

ASCII_MAC = re.compile(rb'\b([0-9A-Fa-f]{2}[:-][0-9A-Fa-f]{2}[:-][0-9A-Fa-f]{2})'
                       rb'(?:[:-][0-9A-Fa-f]{2}){0,3}\b')


def norm(o):
    """Normalise 'aa-bb-cc' / 'AA:BB:CC' -> 'AA:BB:CC'."""
    return o.replace('-', ':').upper()


def load_ieee(path):
    """oui string -> (vendor, address, registry). Handles /24, /28, /36 rows."""
    table = {}
    if not path:
        return table
    with open(path, newline='', encoding='utf-8', errors='replace') as f:
        for r in csv.DictReader(f):
            o = (r.get('oui') or '').strip().upper()
            if not o:
                continue
            table[o] = ((r.get('manufacturer') or '').strip(),
                        (r.get('address') or '').strip(),
                        (r.get('registry') or '').strip())
    return table


def ieee_lookup(ieee, oui24):
    """Try the /24 first, then any longer assignment sharing those 3 bytes."""
    if oui24 in ieee:
        return ieee[oui24]
    for k, v in ieee.items():
        if k.startswith(oui24) and len(k) > len(oui24):
            return v
    return None


def scan_ascii(data):
    """Find ASCII MAC-ish strings. Returns OrderedDict oui24 -> [full matches]."""
    out = OrderedDict()
    for m in ASCII_MAC.finditer(data):
        full = m.group(0).decode('ascii', 'replace')
        o24 = norm(m.group(1).decode('ascii', 'replace'))
        out.setdefault(o24, []).append(full)
    return out


def _looks_sequential(run):
    """
    Reject runs that are just incrementing bytes rather than a real table.

    The low OUI space (00:00:xx, 00:01:xx ...) was assigned sequentially in the
    1980s, so ANY region of counting bytes in a binary will validate as a long
    string of "real" OUIs. Real vendor tables are scattered across the space.
    """
    if len(run) < 3:
        return False
    def as_int(o):
        return int(o.replace(':', ''), 16)
    vals = [as_int(o) for o in run]
    steps = [vals[i + 1] - vals[i] for i in range(len(vals) - 1)]
    inc = sum(1 for s in steps if s == 1)
    if inc >= len(steps) * 0.6:          # mostly counting upward
        return True
    # also reject runs confined to the dense legacy block
    if all(v < 0x000200 for v in vals):
        return True
    return False


def scan_packed(data, ieee, min_run=6):
    """
    Heuristic for packed 3-byte tables: slide over the binary looking for runs
    of consecutive 3-byte groups that are ALL real IEEE /24 assignments. Random
    data almost never produces a long run of valid OUIs, so a hit is meaningful.

    Requires --ieee, and will be noisy without it.
    """
    if not ieee:
        print("packed mode needs --ieee to validate candidates", file=sys.stderr)
        return OrderedDict()

    valid24 = set(k for k in ieee if len(k) == 8)   # 'AA:BB:CC'
    out = OrderedDict()
    n = len(data)

    for stride in (3, 4):        # 3 = tightly packed, 4 = padded/aligned
        i = 0
        while i + stride * min_run <= n:
            run = []
            j = i
            while j + 3 <= n:
                o = "%02X:%02X:%02X" % (data[j], data[j + 1], data[j + 2])
                if o in valid24:
                    run.append(o)
                    j += stride
                else:
                    break
            if len(run) >= min_run and not _looks_sequential(run):
                for o in run:
                    out.setdefault(o, []).append(f"packed(stride={stride})")
                i = j
            else:
                i += 1
    return out


def parse_our_table(path):
    """Pull prefixes + vendor out of our oui_table.cpp."""
    src = open(path, encoding='utf-8').read()
    rows = re.findall(
        r'\{\s*\{\s*((?:0x[0-9A-Fa-f]{2},\s*){4}0x[0-9A-Fa-f]{2})\s*\},\s*(\d+),\s*"([^"]+)"',
        src)
    ours = {}
    for byts, bits, vendor in rows:
        b = [int(x, 16) for x in byts.replace(' ', '').split(',')]
        o24 = "%02X:%02X:%02X" % (b[0], b[1], b[2])
        # Several vendors can share one /24 when the block is split into
        # /28 or /36 assignments (e.g. ELSAG and Perceptics both under
        # 70:B3:D5), so collect a list rather than overwriting.
        ours.setdefault(o24, []).append((vendor, int(bits)))
    return ours


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('binary')
    ap.add_argument('--ieee', help='master_oui.csv for vendor resolution')
    ap.add_argument('--mode', choices=['ascii', 'packed', 'both'], default='ascii')
    ap.add_argument('--compare', help='path to our src/oui_table.cpp')
    ap.add_argument('--min-run', type=int, default=6,
                    help='packed mode: consecutive valid OUIs required')
    args = ap.parse_args()

    data = open(args.binary, 'rb').read()
    print(f"binary: {args.binary}  ({len(data):,} bytes)\n")

    ieee = load_ieee(args.ieee)
    if args.ieee:
        print(f"IEEE entries loaded: {len(ieee):,}\n")

    found = OrderedDict()
    if args.mode in ('ascii', 'both'):
        a = scan_ascii(data)
        print(f"ASCII scan: {len(a)} distinct 3-byte prefixes")
        found.update(a)
    if args.mode in ('packed', 'both'):
        p = scan_packed(data, ieee, args.min_run)
        print(f"packed scan: {len(p)} distinct 3-byte prefixes")
        for k, v in p.items():
            found.setdefault(k, []).extend(v)

    if not found:
        print("\nNothing found. If ASCII mode came up empty, the table may be "
              "packed bytes - try --mode packed. If that also fails, the OUIs "
              "may be computed/obfuscated and you'll need Ghidra.")
        return

    print(f"\n{'PREFIX':<10} {'VENDOR (IEEE)':<38} {'REG':<6} ADDRESS")
    print("-" * 100)
    resolved = {}
    for o24 in sorted(found):
        info = ieee_lookup(ieee, o24) if ieee else None
        if info:
            vend, addr, reg = info
            resolved[o24] = vend
            print(f"{o24:<10} {vend[:36]:<38} {reg:<6} {addr[:40]}")
        else:
            print(f"{o24:<10} {'(not in IEEE registry)':<38} {'':<6}")

    if args.compare:
        ours = parse_our_table(args.compare)
        theirs = set(found)
        mine = set(ours)

        print("\n" + "=" * 100)
        print(f"COMPARISON  (theirs={len(theirs)}  ours={len(mine)})")
        print("=" * 100)

        both = sorted(theirs & mine)
        only_them = sorted(theirs - mine)
        only_us = sorted(mine - theirs)

        print(f"\n--- IN BOTH ({len(both)}) ---")
        for o in both:
            vend = " / ".join(f"{v} (/{b})" for v, b in ours[o])
            print(f"  {o}  ours={vend:<28} ieee={resolved.get(o, '?')[:34]}")

        print(f"\n--- ONLY IN FIRMWARE ({len(only_them)}) - candidates to add ---")
        for o in only_them:
            print(f"  {o}  {resolved.get(o, '(unresolved)')[:60]}")

        print(f"\n--- ONLY IN OURS ({len(only_us)}) - we may be broader ---")
        for o in only_us:
            print(f"  {o}  " + " / ".join(f"{v} (/{b})" for v, b in ours[o]))

        print("\nNOTE: a prefix appearing in their firmware is not automatic "
              "justification to add it. Check the IEEE address column - their "
              "list may include broad-market vendors we excluded on purpose, "
              "or field-observed prefixes that aren't real vendor assignments.")


if __name__ == '__main__':
    main()
