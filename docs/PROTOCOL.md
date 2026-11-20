# Logitech HID++ notes

Everything below was observed on a `046D:C547` receiver with a Logitech
2.4 GHz mouse, Windows 11 build 26200, or is marked as assumed.

## Transport

The receiver exposes two vendor collections:

| Collection | Report size | Answers |
|---|---|---|
| `FF00/0001` | 7 bytes in / 7 out | receiver-level HID++ 1.0 registers |
| `FF00/0002` | 20 bytes in / 20 out | device-level HID++ 2.0 requests |

The device never answered a 7-byte request; the same request sent as a 20-byte
long report was answered immediately. The receiver itself answers on the short
collection. So: **receiver registers → short, device features → long**, and the
implementation simply tries short first and falls back to long.

Opening the mouse and keyboard collections (`0001/0002`, `0001/0006`) fails with
`ERROR_ACCESS_DENIED` because the Windows input stack owns them. This is
expected and irrelevant: battery data lives on the vendor collection.

## Frame layout

```
request       [0]=report id  [1]=device  [2]=feature/register  [3]=function/address  [4..]=params
reply, ok     [0]            [1]=device  [2]=feature/register  [3]=function/address  [4..]=params
reply, error  [0]            [1]=device  [2]=0x8F  [3]=echo of [2]  [4]=echo of [3]  [5]=code
```

Captured frames:

| Frame | Meaning |
|---|---|
| `10 01 41 11 A2 93 40` | unsolicited notification from device 1 after the wake request |
| `10 01 8F 81 0D 01 00` | register read rejected: `0x01` invalid sub-id — the device has no 1.0 registers |
| `10 02 8F 00 10 08 00` | slot 2: `0x08` unknown device (empty slot) |
| `11 01 00 10 04 02` | long ping reply: device 1 speaks HID++ **4.2** |
| `11 01 00 00 06 00 03` | `GetFeature(0x1004)` → feature index `6`, type `0`, version `3` |
| `11 01 06 00 0F 07 01 00` | `0x1004` function 0 → payload `0F 07 01 00` |

Error codes: `0x01` invalid sub-id, `0x02` invalid address, `0x07` busy,
`0x08` unknown device, `0x09` resource error, `0x0B` unsupported parameter.

## Slot scanning

The receiver forwards to paired devices on slots `0x01`..`0x06`, and answers
`0x08` immediately for empty slots. That reply is definitive, so the scan can
skip a slot without trying anything else. A sleeping device instead produces
**silence**, which is why every request needs a time budget of roughly 1.5 s
rather than a single read timeout.

## Battery feature 0x1004

`GetFeature(0x1004)` returned index `6`; function `0` then returned
`0F 07 01 00`. The mouse reported 70% and was not on any kind of power cable.

| Byte | Value | Interpretation |
|---|---|---|
| `[0]` | `0x0F` = 15 | state of charge per the feature table — **contradicts reality** |
| `[1]` | `0x07` | battery level 0..10 → **70%**, matches reality |
| `[2]` | `0x01` | charging status — an AA-powered mouse cannot charge |
| `[3]` | `0x00` | external power |

So the implementation reads `[1] * 10` and only falls back to `[0]` when `[1]`
is outside 0..10. The status bytes are ignored: a device without a charging
circuit still reports `01 00`, so surfacing "charging" from them would be wrong.

The cost of this choice is granularity: a 0..10 level only moves in 10% steps.
If a later device reports a plausible state-of-charge byte and a level that
disagrees, the two must be compared against a known battery reading before
either is trusted.

## HID++ 1.0 registers

Only the receiver index (`0xFF`) answered register reads.

| Direction | Sub-id | Address |
|---|---|---|
| read | `0x81` | register |
| write | `0x80` | register |

Register `0x02` (connection state) returns payload `00 01 00`. **Assumed:** the
first byte is the connected count and the second the slot limit. Writing `0x02`
triggers the receiver to ping its paired devices — that is what produced the
notification in the table above.

## Verified vs assumed

| Item | Status |
|---|---|
| Receiver answers 1.0 registers on the short collection | verified |
| Device answers 2.0 requests only as long reports | verified |
| Error frame echo layout | verified against three captures |
| Slot scan and the `0x08` empty-slot reply | verified |
| Feature discovery and `0x1004` read | verified |
| Battery percentage from `0x1004` byte 1 | verified against a known 70% |
| `0x1004` byte 0 and the status bytes | unclear, deliberately unused |
| Register `0x02` byte layout | assumed |
| Bolt (encrypted) receivers | not investigated |
| HID++ 1.0 battery registers `0x07` / `0x0D` | implemented, never answered by this device |
