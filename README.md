# Yoto card preparation with an ACR122U on macOS

These instructions are for two card layouts. Each layout has **48 bytes of user memory**:

| Card layout | Full readable dump | Pages |
| --- | --- | --- |
| Original / legacy-compatible MIFARE Ultralight | **64 bytes** | 0–15 |
| MIFARE Ultralight EV1 MF0UL11 | **80 bytes** | 0–19 |

Both layouts keep their 48 content bytes at offsets 16–63.

The legacy card does not have the GET_VERSION command. Before the helper sends
GET_VERSION, it finds the memory rollover of the card. Thus, the helper can
examine the two layouts.

NOTE: The helper identifies the layout. It does not prove that the chip is
authentic.

The format procedure prepares a blank NFC Forum Type 2 / NDEF card.

## Copy one card to another

Build `card-tool` before you do this procedure. Refer to the "Setup" section.

CAUTION: READ THE SOURCE CARD FIRST. THEN REMOVE IT AND PUT THE DESTINATION
CARD ON THE READER. FORMAT ONLY THE DESTINATION CARD. IF YOU FORMAT THE SOURCE
CARD, YOU ERASE ITS CONTENT.

### A. Save the source card

1. Put the card that has the playlist on the reader.
2. Do these commands:

```bash
cd ~/src/yoto
export LIBNFC_DEFAULT_DEVICE=acr122_usb
yoto_tmp="$(mktemp -d "${TMPDIR:-/tmp}/yoto.XXXXXX")"
./card-tool inspect
nfc-mfultralight r "$yoto_tmp/source.mfd"
wc -c "$yoto_tmp/source.mfd"
printf 'Source saved in: %s\n' "$yoto_tmp/source.mfd"
```

3. Make sure that the output shows **zero failed pages**.
4. Make sure that the file size is the same as the size that `inspect` shows:
   - 64 bytes for the legacy card.
   - 80 bytes for the EV1 card.
5. Keep this terminal open.

### B. Put in the destination card, format it, and copy the content

1. Remove the source card.
2. Put only the destination card on the reader.
3. Do this command:

```bash
./card-tool inspect
```

4. Find the **destination UID** in the output.

CAUTION: USE THE UID OF THE DESTINATION CARD. DO NOT USE THE UID OF THE
SOURCE CARD.

5. Do these commands. Replace the example value with the destination UID:

```bash
target_uid=REPLACE_WITH_DESTINATION_UID
./card-tool format "$target_uid" "$yoto_tmp/destination-before-format.mfd" &&
./card-tool write "$target_uid" "$yoto_tmp/source.mfd" "$yoto_tmp/destination-before-write.mfd"
```

The `&&` operator stops the copy if the format command fails.

The format command does these steps:

- It erases the destination card.
- It writes the NDEF header if it is necessary.
- It does a check of the result.

The write command copies only the 48-byte content area. Then it does a check of
the result.

The two commands make backups. The destination card keeps its own UID and its
own lock and configuration settings. The commands do not change the saved
source file.

NOTE: The helper does not overwrite a backup file that exists. For each
subsequent destination card, use a new backup filename for each command.

This procedure copies the NFC content. It does not copy the audio files or the
Yoto account authorization. After the copy, play the card in the player to make
sure that it operates correctly.

## Setup

1. Install the tools if they are not installed.
2. Build the helper:

```bash
brew install libnfc nfcutils python
cd ~/src/yoto
cc -Wall -Wextra -Werror \
  -I"$(brew --prefix libnfc)/include" \
  -L"$(brew --prefix libnfc)/lib" \
  card-tool.c -lnfc -o card-tool
```

3. Close all other NFC applications.
4. Put **one card** on the reader.
5. Do these commands. They select the USB backend and make a temporary work
   directory:

```bash
export LIBNFC_DEFAULT_DEVICE=acr122_usb
yoto_tmp="$(mktemp -d "${TMPDIR:-/tmp}/yoto.XXXXXX")"
printf 'Temporary files: %s\n' "$yoto_tmp"
nfc-list -v -t 1
```

NOTE: The USB backend operated correctly on the test Mac.

Use the same terminal for all subsequent steps. Thus, `yoto_tmp` stays
defined. If you open a new terminal, do the `export` and `yoto_tmp` commands
again.

CAUTION: MACOS CAN REMOVE THE TEMPORARY DIRECTORY. MOVE THE DUMPS THAT YOU
WANT TO KEEP TO A DIFFERENT DIRECTORY.

The `nfc-list` command shows the UID and the general tag family. To find the
exact chip and its format header, do this command:

```bash
./card-tool inspect
```

The helper accepts these layouts:

- The legacy 48-byte rollover layout.
- The EV1 version `00 04 03 01 01 00 0b 03`.

The helper uses the first ACR122U USB reader. It does not accept other layouts
or versions.

The write commands of the helper must have the **destination UID**. Write the
UID in lowercase hexadecimal, with no spaces and no colons. Get the UID from
the `inspect` output. Do not use the UID of a different card.

## 1. Format a 48-byte card

CAUTION: THE FORMAT COMMAND **ERASES THE 48-BYTE CONTENT AREA**.

Before the helper changes the card, it saves a full backup of 64 or 80 bytes.
The helper does not overwrite a backup file that exists.

1. Put the card that you want to format on the reader.
2. Do these commands. Replace the example value with the UID that `inspect`
   shows:

```bash
./card-tool inspect
# Replace the example value below with the UID printed by inspect.
target_uid=REPLACE_WITH_TARGET_UID
./card-tool format "$target_uid" "$yoto_tmp/before-format.mfd"
```

The helper does these steps:

- It does a check of the chip, the destination UID, the lock bits, and the
  protection settings.
- It sets page 3 to `E1 10 06 00`. This value identifies a 48-byte NDEF data
  area that you can write.
- It writes `03 00 FE` at the start of that area. This is an empty NDEF TLV
  and a terminator. It writes zeros in the remaining bytes.
- It does not change pages 0–2 (UID, manufacturer, and lock data).
- On EV1 cards, it does not change pages 16–19 (configuration).
- It opens the reader again and compares all readable bytes with the
  expected result.

CAUTION: **YOU CAN PROGRAM PAGE 3 ONLY ONE TIME.** YOU CANNOT CLEAR A BIT
AFTER YOU SET IT.

You can write the content area again after the format procedure. The helper
accepts a blank page 3 or the correct header. It does not accept other values.

NOTE: If you format a card that has a format already, the helper erases its
content.

If the procedure is successful, the helper shows one of these messages:

- `Verified all 64 readable bytes`
- `Verified all 80 readable bytes`

If a communication failure or a check failure occurs, some writes possibly
completed. Read the card again before you try the format again. Use a new
backup filename for each attempt.

## 2. Read a card into a temporary MFD file

1. Put the **source card** on the reader.
2. Do these commands:

```bash
nfc-mfultralight r "$yoto_tmp/source.mfd"
wc -c "$yoto_tmp/source.mfd"
xxd "$yoto_tmp/source.mfd"
```

3. Make sure that the dump has the correct size:
   - Legacy cards: **16 pages / 64 bytes**.
   - `MF0UL11 (48 bytes)` cards: **20 pages / 80 bytes**.
4. Make sure that the output shows **zero failed pages**.
5. Do `./card-tool inspect` to make sure that the layout is correct.

NOTE: If version detection fails, the stock reader can read only 16 pages.
Thus, the dump length does not prove the chip type.

CAUTION: DO NOT USE A PARTIAL READ AS A SOURCE. THE EXIT STATUS OF THE TOOL
DOES NOT PROVE THAT THE READ IS COMPLETE.

An MFD file is raw binary memory. It is not a text file or an audio file. The
layout for these cards is:

| Pages | Byte offsets | Contents |
| --- | --- | --- |
| 0–2 | 0–11 | UID, manufacturer information, static lock bits |
| 3 | 12–15 | NDEF capability container / OTP |
| 4–15 | 16–63 | 48-byte user content area |
| 16–19 (EV1 only) | 64–79 | Configuration, password, and PACK fields |

A read does not show a hidden password. The utility can mask or replace the
password and PACK bytes in the dump. Thus, do not restore these fields from a
dump.

To keep a source after the temporary session, copy it to a permanent filename:

```bash
cp "$yoto_tmp/source.mfd" ~/src/yoto/my-playlist.mfd
```

## 3. Write the content of an MFD file to a card

Make sure that these conditions are correct before you start:

- The destination card has one of the two 48-byte layouts.
- The destination card has a format from step 1.
- The source is a dump of 64 or 80 bytes with the header `E1 10 06 00`.

You can copy content between the two layouts. The helper never copies the
configuration.

1. Put the **destination card** on the reader.
2. Do these commands. Replace the example value with the UID of the
   destination card:

```bash
./card-tool inspect
# Replace this with the DESTINATION card's UID, not the source card's UID.
target_uid=REPLACE_WITH_TARGET_UID
./card-tool write "$target_uid" "$yoto_tmp/source.mfd" "$yoto_tmp/before-write.mfd"
```

For a saved source, use its path:

```bash
./card-tool write "$target_uid" ~/src/yoto/my-playlist.mfd "$yoto_tmp/before-write-saved.mfd"
```

NOTE: These two write commands are alternatives. Do only the command that
agrees with your source.

The helper does these steps:

- It makes a backup of the destination card.
- It writes **only pages 4–15**.
- It does a check of the result.

The helper does not copy the source UID, header, locks, or configuration. The
destination card keeps its own identity. If you could write to the card before,
you can write to it after.

To do an independent read and comparison of the content, do these commands:

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

If you used a saved source, use the same source path in the comparison.

NOTE: Do not use `cmp` to compare the full files. The UIDs of different cards
are not the same.

### Why you must use the helper to write

The stock command is `nfc-mfultralight w FILE.mfd`. Its write loop can write to
the EV1 configuration and password pages. This can occur also when you refuse
UID and lock writes.

The `--partial` option only removes the input-size check. It does **not**
limit the writes to the pages in a short dump. The helper limits content
writes to pages 4–15.

CAUTION: DO NOT USE `--full`, `--uid`, `--lock`, OR `--dynlock` FOR THIS
PROCEDURE.

## Reader troubleshooting

Do these commands to examine the reader:

```bash
nfc-scan-device -v
LIBNFC_DEFAULT_DEVICE=acr122_usb nfc-list -v -t 1
```

- If the reader opens but does not find a tag:
  1. Put one card at the center of the reader.
  2. Remove the card.
  3. Put the card on the reader again.
- If the reader does not open:
  1. Close all other NFC applications.
  2. Disconnect the reader and connect it again.
  3. Try again.
- USB addresses such as `acr122_usb:002:004` can change after you connect the
  reader again. The general `acr122_usb` selector does not use a fixed address.
- A general `MIFARE Ultralight` result from `nfc-list` does not show the
  capacity. To find the capacity, use `card-tool inspect` or the detailed
  `nfc-mfultralight` read output.
- Protected or locked cards need a different procedure. The helper does not
  accept these cards.

## References and verification scope

- [libnfc configuration and environment variables](https://github.com/nfc-tools/libnfc#environment-variables)
- [Upstream nfc-mfultralight implementation](https://github.com/nfc-tools/libnfc/blob/master/utils/nfc-mfultralight.c)
- [NXP original Ultralight datasheet](https://www.nxp.com/docs/en/data-sheet/MF0ICU1.pdf)
- [NXP MF0ULx1 datasheet](https://www.nxp.com/docs/en/data-sheet/MF0ULX1.pdf)
- [Type-specific NDEF formatting notes](https://github.com/RfidResearchGroup/proxmark3/blob/master/doc/mfu_ndef_format_notes.md)
- [Yoto's supported Make Your Own process](https://uk.yotoplay.com/make-your-own)

The reader and the original card were tested with Homebrew libnfc 1.8.0 on
Apple Silicon macOS. The write and the full read of the earlier one-card
formatter were successful. This helper uses the same commands.

The preparation of this document did not format or write to a card. A
playback test on a Yoto player is a separate check.

The legacy-layout detection was tested on UID `0453e77f780000`. This test only
read the card. The card had an NDEF header and content already. This
compatibility change did not format or write to that card.

NOTE: After you change the source code, build `card-tool` again.
