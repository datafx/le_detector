#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""
decrypt_nyan_ouis.py - recover the LE Gear Detector's OUI list from a nyanBOX
ESP32 flash dump.

nyanBOX stores its LE-gear OUI watchlist XOR-obfuscated in flash and decrypts it
at runtime in FUN_400f779c. This reproduces that decryption so the list can be
inspected. The (key_hi, key_lo, data_vaddr, length) tuples below were read out of
that function with Ghidra; if a future firmware revision changes the list you'll
need to re-read them.

Keystream per entry = key_hi.to_bytes(4,'little') + key_lo.to_bytes(4,'little'),
XORed against the ciphertext at data_vaddr. Result is an ASCII OUI (6 hex chars,
or 7 for a /28 assignment).

Usage:  python3 decrypt_nyan_ouis.py nyan_dump.bin
"""
import sys

# vaddr -> file offset for the DROM segment of a single-app ESP32 image
def v2f(v): return v - 0x3F400000 + 0x10000

ENTRIES = [
 (0xad1fd949,0xa95daba1,0x3f424613,7),(0x77e7bfb5,0xe38d7743,0x3f42460c,7),
 (0x47a3dd4d,0x59437fd5,0x3f424605,7),(0x5db90def,0x4ba34d47,0x3f4245fd,8),
 (0xf98bfd3d,0x9173cf81,0x3f4245f6,7),(0x9977bd19,0x834b8777,0x3f4245ef,7),
 (0xcd6be56b,0x5361499f,0x3f4245e8,7),(0xe3d5dfd9,0xb12fcdb5,0x3f4245e1,7),
 (0x33d1c15d,0xbdfde331,0x3f4245da,7),(0x41b7d3f5,0xdd0d1511,0x3f4245d3,7),
 (0x0973894d,0xdd19f973,0x3f4245cc,7),(0xa7437325,0x0d0bef3d,0x3f4245c5,7),
 (0x3de591e3,0xdb3f45e9,0x3f4245be,7),(0x2de1a571,0x7377b9d3,0x3f4245b7,7),
 (0xc1e5c5d3,0x250d7dc5,0x3f4245b0,7),(0xbfe3e379,0x23d50113,0x3f4245a9,7),
 (0x6b6fff53,0x4f09ff71,0x3f4245a2,7),(0x9d815b2b,0x41330b53,0x3f42459b,7),
 (0xe761df7d,0xcb77d973,0x3f424594,7),(0x059d81e3,0x457f511d,0x3f42458d,7),
 (0x891f53ef,0x1f17b183,0x3f424586,7),(0x3b51cbb7,0x19e1fbad,0x3f42457f,7),
 (0x793f2f45,0x1bcd9b4d,0x3f424578,7),(0x09afe71f,0x6d97d7c1,0x3f424571,7),
 (0x8369a585,0xf54917eb,0x3f42456a,7),(0xf5b9bd7b,0x8d8bf1bf,0x3f424563,7),
 (0xfd45c15f,0xf7632581,0x3f42455c,7),(0xb9991b69,0x8d77af53,0x3f424555,7),
 (0xe311e577,0xd34ba995,0x3f42454e,7),(0x11c9ab71,0x4707a117,0x3f424547,7),
 (0x2fab3b83,0x5dd36969,0x3f424540,7),(0x816bbf85,0xa5fb0b19,0x3f424539,7),
 (0xf54fd385,0xd76f9131,0x3f424532,7),(0x812b075d,0xd39d932b,0x3f42452b,7),
 (0xff95074b,0xc3c94d3d,0x3f424524,7),(0x79c34bbf,0x8f0d4ba1,0x3f42451d,7),
 (0xb965656d,0xa359df37,0x3f424516,7),(0x37db55af,0x4f832dad,0x3f42450f,7),
 (0x5f79eb3d,0x87432381,0x3f424508,7),(0x2717dd1f,0xb9fdd3e7,0x3f424501,7),
 (0x09197d01,0xf1c9cfcf,0x3f4244fa,7),(0x09937197,0x5955cf27,0x3f4244f3,7),
 (0x9775a1e5,0xc98371e5,0x3f4244ec,7),(0xb1e50fff,0xe539030b,0x3f4244e5,7),
 (0x0b2f772d,0xcf7b2701,0x3f4244dd,8),(0xa3a1dd33,0x499b0df9,0x3f4244d6,7),
 (0xd5d53b11,0xcd1fcd15,0x3f4244cf,7),(0xab1ffb21,0x8d4121e1,0x3f4244c8,7),
 (0x21398b5d,0x0fbf0517,0x3f4244c0,8),
]

def main():
    if len(sys.argv) != 2:
        print(__doc__); sys.exit(1)
    d = open(sys.argv[1], 'rb').read()
    out = []
    for hi, lo, addr, cnt in ENTRIES:
        ks = hi.to_bytes(4,'little') + lo.to_bytes(4,'little')
        f = v2f(addr)
        s = bytes(d[f+i] ^ ks[i] for i in range(cnt)).rstrip(b'\x00').decode('ascii','replace')
        out.append(s)
    print(f"{len(out)} OUIs recovered:\n")
    for i, s in enumerate(out):
        print(f"  [{i:2}] {s}")

if __name__ == '__main__':
    main()
