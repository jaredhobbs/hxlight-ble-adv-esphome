#!/usr/bin/env python3
"""Parse a raw HXLight/JOOFO BLE advertisement into ESPHome config values."""

import sys

ON_BODY = "632ce9b951fb2b4b198159"
OFF_BODY = "632ce9b951fb284b198159"
BRIGHTNESS_PREFIX = "662ce9b9549f"
CCT_PREFIX_1 = "662ce9b951f4"
CCT_PREFIX_2 = "662ce9b9569f"


def die(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)


def main() -> None:
    if len(sys.argv) != 2:
        die("usage: parse_raw.py <raw hex advertisement>")

    raw_hex = sys.argv[1].strip().replace(" ", "").replace(":", "").replace("-", "").lower()
    try:
        raw = bytes.fromhex(raw_hex)
    except ValueError:
        die("input is not valid hex")

    if len(raw) != 31:
        die(f"expected 31 bytes, got {len(raw)}")
    if raw[0:2] != b"\x02\x01":
        die("not a BLE flags-prefixed advertisement")
    if raw[3:7] != bytes.fromhex("1bfff0ff"):
        die("not an HXLight f0ff manufacturer-data advertisement")
    if raw[-1] != 0x18:
        die("unexpected final byte; expected 0x18")

    prefix = raw[7:15].hex()
    body = raw[15:26].hex()
    tail0 = raw[26]
    tail1 = raw[27]
    observed_seq = tail0 ^ 0xB6
    next_seq = (observed_seq + 1) & 0xFF

    if body == ON_BODY:
        command = "on"
    elif body == OFF_BODY:
        command = "off"
    elif body.startswith(BRIGHTNESS_PREFIX):
        level = raw[21] ^ 0x2A
        command = f"brightness {level}%"
    elif body.startswith(CCT_PREFIX_1) or body.startswith(CCT_PREFIX_2):
        # For the known CCT slider body, byte 21 is cold ^ 0x2a.
        cold = raw[21] ^ 0x2A
        command = f"color temperature, cold={cold}%"
    else:
        command = "unknown"

    print(f"device_prefix: {prefix}")
    print(f"observed_sequence: 0x{observed_seq:02x} / {observed_seq}")
    print(f"next_initial_sequence: 0x{next_seq:02x} / {next_seq}")
    print(f"tail0: 0x{tail0:02x}")
    print(f"tail1: 0x{tail1:02x}")
    print(f"command_guess: {command}")


if __name__ == "__main__":
    main()
