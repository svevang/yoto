# Yoto card preparation with an ACR122U on macOS

These instructions support two layouts with **48 bytes of user memory**:

| Card layout | Full readable dump | Pages |
| --- | --- | --- |
| Original / legacy-compatible MIFARE Ultralight | **64 bytes** | 0–15 |
| MIFARE Ultralight EV1 MF0UL11 | **80 bytes** | 0–19 |

Both store their 48 content bytes at offsets 16–63. The older card does not
implement GET_VERSION. The helper identifies its memory rollover before trying
that command, so it can inspect both layouts. This identifies the layout, not
cryptographic chip authenticity.

Formatting initializes a blank NFC Forum Type 2 / NDEF card.

## Copy one card to another

After building `card-tool` using the setup instructions below, use this sequence.
**Read the source first, then swap cards. Only format the destination.**

### A. Save the source card

Place the card containing the playlist on the reader:

```bash
cd ~/src/yoto
export LIBNFC_DEFAULT_DEVICE=acr122_usb
yoto_tmp="$(mktemp -d "${TMPDIR:-/tmp}/yoto.XXXXXX")"
./card-tool inspect
nfc-mfultralight r "$yoto_tmp/source.mfd"
wc -c "$yoto_tmp/source.mfd"
printf 'Source saved in: %s\n' "$yoto_tmp/source.mfd"
```

Confirm **zero failed pages** and the size reported by `inspect`: 64 bytes for
the legacy card or 80 bytes for EV1. Keep this terminal open.

### B. Swap in the destination, format it, and copy the content

Remove the source card and place only the destination card on the reader:

```bash
./card-tool inspect
```

Use the **new destination UID** from that output, not the source UID:

```bash
target_uid=REPLACE_WITH_DESTINATION_UID
./card-tool format "$target_uid" "$yoto_tmp/destination-before-format.mfd" &&
./card-tool write "$target_uid" "$yoto_tmp/source.mfd" "$yoto_tmp/destination-before-write.mfd"
```

The `&&` prevents copying if formatting fails. Formatting clears the destination,
initializes its NDEF header if needed, and verifies the result. Writing copies
only the 48-byte content area and verifies it. Both commands make backups;
the destination retains its own UID and lock/configuration settings. The saved
source file is not changed. For the next destination, use a new temporary backup
filename for each command; existing backups are deliberately not overwritten.

This copies the NFC content, not the audio files or Yoto account authorization.
Test playback in the player afterward.

## Setup

Install the tools if needed, then build the included helper:

```bash
brew install libnfc nfcutils python
cd ~/src/yoto
cc -Wall -Wextra -Werror \
  -I"$(brew --prefix libnfc)/include" \
  -L"$(brew --prefix libnfc)/lib" \
  card-tool.c -lnfc -o card-tool
```

Close other NFC apps and place **one card** on the reader. Select its USB
backend, which worked on this Mac, and create a temporary working directory:

```bash
export LIBNFC_DEFAULT_DEVICE=acr122_usb
yoto_tmp="$(mktemp -d "${TMPDIR:-/tmp}/yoto.XXXXXX")"
printf 'Temporary files: %s\n' "$yoto_tmp"
nfc-list -v -t 1
```

Keep using the same terminal so `yoto_tmp` remains defined. If you open a new
terminal, repeat the environment and temporary-directory commands. Move any
dumps you want to keep out of the temporary directory before macOS removes it.

`nfc-list` displays the UID and general tag family. For a precise check of the
supported chip and its formatting header, run:

```bash
./card-tool inspect
```

The helper accepts the legacy 48-byte rollover layout or the verified EV1
version `00 04 03 01 01 00 0b 03`. It uses the first ACR122U USB reader and
refuses other layouts/versions. Its write commands
require the **destination UID**, in lowercase hex without spaces or colons.
Read that value from `inspect`; do not reuse another card's UID.

## 1. Format a 48-byte card

Formatting **erases the 48-byte content area**. The helper automatically saves
a full 64- or 80-byte backup before changing anything and refuses to overwrite an existing
backup file.

With the card to format on the reader:

```bash
./card-tool inspect
# Replace the example value below with the UID printed by inspect.
target_uid=REPLACE_WITH_TARGET_UID
./card-tool format "$target_uid" "$yoto_tmp/before-format.mfd"
```

The helper:

- Checks the chip, destination UID, lock bits, and protection settings.
- Sets page 3 to `E1 10 06 00`, declaring a writable 48-byte NDEF data area.
- Initializes that area with `03 00 FE` (empty NDEF TLV and terminator), then zeros.
- Preserves pages 0–2 (UID/manufacturer/lock data) and, on EV1, 16–19 (configuration).
- Reopens the reader and verifies all readable bytes against the expected result.

**Page 3 is one-time programmable:** bits set there cannot be cleared again.
The content area remains rewritable. The helper accepts a blank page 3 or the
already-correct header and refuses incompatible values. Formatting an already
formatted card still erases its content.

Expect `Verified all 64 readable bytes` or `Verified all 80 readable bytes` on success. If communication or
verification fails, read the card again before retrying: some writes may already
have succeeded. Use a new backup filename for another attempt.

## 2. Read a card into a temporary MFD file

Place the **source card** on the reader:

```bash
nfc-mfultralight r "$yoto_tmp/source.mfd"
wc -c "$yoto_tmp/source.mfd"
xxd "$yoto_tmp/source.mfd"
```

For legacy cards expect **16 pages / 64 bytes**; for `MF0UL11 (48 bytes)`
expect **20 pages / 80 bytes**. Require **zero failed pages**. Confirm the layout
with `./card-tool inspect`; the stock reader can fall back to 16 pages when
version detection fails, so dump length alone does not prove the chip type. Do not use a partial read as a
source; the tool's exit status alone is not enough to establish a complete read.

An MFD is raw binary memory, not a text or audio file. The layout for these cards is:

| Pages | Byte offsets | Contents |
| --- | --- | --- |
| 0–2 | 0–11 | UID, manufacturer information, static lock bits |
| 3 | 12–15 | NDEF capability container / OTP |
| 4–15 | 16–63 | 48-byte user content area |
| 16–19 (EV1 only) | 64–79 | Configuration, password, and PACK fields |

Reading does not recover a hidden password. The dump's password/PACK bytes may
be masked or substituted by the utility; this is another reason not to restore
those fields from a dump.

To retain a source beyond the temporary session, choose a permanent filename:

```bash
cp "$yoto_tmp/source.mfd" ~/src/yoto/my-playlist.mfd
```

## 3. Write an MFD's content to a card

Place the **destination card** on the reader. It must have one of the supported
48-byte layouts and already be formatted using step 1. The source must be a
64- or 80-byte dump with header `E1 10 06 00`. Content can be copied between
the two supported layouts; configuration is never copied.

```bash
./card-tool inspect
# Replace this with the DESTINATION card's UID, not the source card's UID.
target_uid=REPLACE_WITH_TARGET_UID
./card-tool write "$target_uid" "$yoto_tmp/source.mfd" "$yoto_tmp/before-write.mfd"
```

For a saved source, substitute its path:

```bash
./card-tool write "$target_uid" ~/src/yoto/my-playlist.mfd "$yoto_tmp/before-write-saved.mfd"
```

These are alternative commands; run the one matching your source. The helper
backs up the destination, writes **only pages 4–15**, and verifies the result.
It does not copy the source UID, header, locks, or configuration. The destination
keeps its own identity and stays writable if it was writable before.

For an independent readback and content comparison:

```bash
nfc-mfultralight r "$yoto_tmp/readback.mfd"
python3 - "$yoto_tmp/source.mfd" "$yoto_tmp/readback.mfd" <<'PY'
import sys
from pathlib import Path
source, actual = (Path(p).read_bytes() for p in sys.argv[1:])
if len(source) not in (64, 80) or len(actual) not in (64, 80):
    raise SystemExit("Expected complete 64- or 80-byte dumps")
if source[16:64] != actual[16:64]:
    raise SystemExit("Content verification FAILED")
print("All 48 content bytes match")
PY
```

If you used a saved source, substitute that same source path in the comparison.
A whole-file `cmp` is inappropriate for different cards because their UIDs differ.

### Why use the helper for writing?

The stock syntax is `nfc-mfultralight w FILE.mfd`, but its write loop can reach
EV1 configuration/password pages even when you decline UID and lock writes.
`--partial` relaxes the input-size check; it does **not** restrict writes to the
pages present in a shortened dump. The included helper explicitly restricts
content writes to pages 4–15. Do not use `--full`, `--uid`, `--lock`, or
`--dynlock` for this workflow.

## Reader troubleshooting

```bash
nfc-scan-device -v
LIBNFC_DEFAULT_DEVICE=acr122_usb nfc-list -v -t 1
```

- If the reader opens but no tag is found, center one card on it and lift/reseat it.
- If opening fails, close competing NFC apps, unplug/replug the reader, and retry.
- USB addresses such as `acr122_usb:002:004` can change after reconnecting.
  The generic `acr122_usb` selector avoids hardcoding that address.
- A generic `MIFARE Ultralight` result from `nfc-list` does not identify the
  capacity; use `card-tool inspect` or the detailed `nfc-mfultralight` read output.
- Protected or locked cards require a separate workflow; this helper refuses them.

## References and verification scope

- [libnfc configuration and environment variables](https://github.com/nfc-tools/libnfc#environment-variables)
- [Upstream nfc-mfultralight implementation](https://github.com/nfc-tools/libnfc/blob/master/utils/nfc-mfultralight.c)
- [NXP original Ultralight datasheet](https://www.nxp.com/docs/en/data-sheet/MF0ICU1.pdf)
- [NXP MF0ULx1 datasheet](https://www.nxp.com/docs/en/data-sheet/MF0ULX1.pdf)
- [Type-specific NDEF formatting notes](https://github.com/RfidResearchGroup/proxmark3/blob/master/doc/mfu_ndef_format_notes.md)
- [Yoto's supported Make Your Own process](https://uk.yotoplay.com/make-your-own)

The reader and original card were tested with Homebrew libnfc 1.8.0 on Apple
Silicon macOS. The earlier one-card formatter's write and full readback succeeded.
This reusable helper is based on those commands; documentation preparation does
not itself format or overwrite a card. Yoto playback is a separate verification.

The legacy-layout detection was checked read-only on UID `0453e77f780000`.
That card already had an NDEF header and content; it was not formatted or written
during this compatibility fix. Rebuild `card-tool` after updating the source.
