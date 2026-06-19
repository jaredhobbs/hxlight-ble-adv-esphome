# Protocol notes

This protocol was derived from a JOOFO CCT floor lamp controlled by the HXLight Android app.

Known BLE advertisement wrapper:

```text
02 01 <flags> 1b ff f0 ff <prefix:8> <body:11> <tail0> <tail1> <crc_lo> <crc_hi> 18
```

Known transforms:

```text
body = payload11 XOR e5 1d fe b8 51 fa 2a b4 e7 d4 0c
tail0 = seq XOR b6
tail1 = checksum XOR 2e
checksum = sum(payload11 + seq) & ff
crc = crc16_x25(prefix + body + tail0 + tail1) XOR 6a4d, little-endian
```

Known app payloads:

```text
ON:         86 31 17 01 00 01 01 ff fe 55 55 seq checksum
OFF:        86 31 17 01 00 01 02 ff fe 55 55 seq checksum
Brightness: 83 31 17 01 05 65 <level> fa 9a 55 55 seq checksum
CCT:        83 31 17 01 07 65 <cold> <warm> 9a 55 55 seq checksum
```
