# tools/

## extract_oui.py

Pulls MAC/OUI prefixes out of a firmware binary and diffs them against
`src/oui_table.cpp`.

### Getting the firmware

The nyanBOX web flasher is ESP Web Tools, which reads a `manifest.json` listing
the actual `.bin` parts. In your browser:

1. Open <https://nyandevices.com/flasher/>
2. DevTools → Network → reload → filter for `manifest` / `.bin`
3. Note the `.bin` URLs and download them (`curl -O`)

Or view-source the flasher page and look for the `manifest=` attribute on the
`<esp-web-install-button>` element.

There may be several parts (bootloader, partition table, app). The **app**
partition is the large one — that's where the tables live.

### Getting the IEEE database

    git clone --depth 1 https://github.com/Ringmast4r/OUI-Master-Database
    # use OUI-Master-Database/LISTS/master_oui.csv

### Running it

    # ASCII scan first - most likely to work
    python3 extract_oui.py firmware.bin --ieee master_oui.csv

    # if that finds nothing, packed-byte heuristic
    python3 extract_oui.py firmware.bin --ieee master_oui.csv --mode packed

    # full comparison against our table
    python3 extract_oui.py firmware.bin --ieee master_oui.csv \
        --mode both --compare ../src/oui_table.cpp

### Why ASCII mode should work

nyanBOX's published detectors store OUIs as string literals, not packed bytes:

    // flock_detector.cpp
    const char* mac_prefixes[] = { "58:8e:81", "cc:cc:cc", ... };

    // axon_detector.cpp
    strncasecmp(addrStr, "00:25:df", 8)

If the LE Gear Detector follows the same pattern the table is plain text in the
binary and needs no disassembly. If both modes come up empty, the prefixes are
computed or obfuscated and you're into Ghidra territory.

### Packed mode false positives

It requires a run of `--min-run` (default 6) *consecutive* valid IEEE
assignments. Random data essentially never produces that. Tested against 200 KB
of random bytes plus a planted 7-entry table: found the 7, zero false positives.
Lower `--min-run` if you suspect a short table, and expect noise if you do.

### Reading the output

`--compare` gives three buckets:

- **IN BOTH** — agreement, good sign for both lists
- **ONLY IN FIRMWARE** — candidates, but *check the IEEE address column first*.
  Their list may include broad-market vendors we excluded deliberately (Dell,
  Panasonic) or field-observed prefixes that aren't real vendor assignments
  (flock_detector.cpp ships `cc:cc:cc`, which is a well-known junk prefix).
- **ONLY IN OURS** — where we're broader than them

A prefix being in their firmware is evidence, not justification. Every addition
should still get the IEEE address check that the rest of our table got.

### Note on the binary

The legacy source in their repo is MIT. The current compiled firmware is not
covered by that licence. The OUI values themselves are public IEEE registry
facts rather than anyone's original expression, but downloading and analysing
their binary is your call to make.

## decrypt_nyan_ouis.py

Recovers the nyanBOX LE Gear Detector's OUI watchlist, which is XOR-obfuscated in
firmware. Point it at a flash dump:

    python3 decrypt_nyan_ouis.py nyan_dump.bin

The per-entry XOR keys and data addresses were read out of the decrypt routine
(FUN_400f779c) with Ghidra. If a firmware update changes the list, re-read those
tuples from that function and update the ENTRIES table in the script. See the
Provenance section of the main README for the full extraction path.
