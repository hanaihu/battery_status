# Logitech HID++ notes

Everything below was observed on a `046D:C547` receiver on Windows 11 build 26200,
or is marked as assumed. Assumptions are kept explicit so the first run against a
real device can confirm or correct them.

## Transport

The receiver exposes two vendor collections:

| Collection | Report size | Behaviour |
|---|---|---|
| `FF00/0001` | 7 bytes in / 7 out | answers HID++ requests |
| `FF00/0002` | 20 bytes in / 20 out | never answered on this machine |

Requests are written with `WriteFile` and replies read with `ReadFile` on an
overlapped handle. The 7-byte collection uses report id `0x10`; the 20-byte one
would use `0x11`. Only the 7-byte path is implemented.

Reads must be filtered: unsolicited notifications and late replies from earlier
requests arrive on the same pipe. A reply is accepted only when its device byte
matches the request.

## Frame layout

```
request       [0]=report id  [1]=device  [2]=feature/register  [3]=function/address  [4..]=params
reply, ok     [0]            [1]=device  [2]=feature/register  [3]=function/address  [4..]=params
reply, error  [0]            [1]=device  [2]=0x8F              [3]=feature/register  [4]=function/address  [5]=code
```

The device byte is the receiver itself for `0xFF`, or the paired-device slot for
`0x01`..`0x06`.

Captured frames:

| Frame | Meaning |
|---|---|
| `10 FF 8F 00 10 01 00` | receiver rejects HID++ 2.0 root `GetProtocolVersion`, code `0x01` invalid sub-id |
| `10 01 8F 00 10 08 00` | slot 1 answers `0x08` unknown device — empty slot, or the device is asleep |
| `10 FF 81 02 00 01 00 00` | HID++ 1.0 read of register `0x02` |

Error codes: `0x01` invalid sub-id, `0x02` invalid address, `0x07` busy,
`0x08` unknown device, `0x09` resource error, `0x0B` unsupported parameter.

## HID++ 1.0 registers

Only the receiver index (`0xFF`) answered register reads here.

| Direction | Sub-id | Address |
|---|---|---|
| read | `0x81` | register |
| write | `0x80` | register |

Register `0x02` (connection state) returned payload `00 01 00 00` while no device
was connected. **Assumed:** the first byte is the connected device count and the
second the slot limit. Not confirmed — needs a capture taken with a device awake,
where the first byte is expected to change.

Known registers used by other clients: `0x07` battery status, `0x0D` battery
charge, `0x02` connection state, `0x2B5` receiver info (long register, unreachable
here because the 20-byte collection does not answer).

## HID++ 2.0 features

Feature index `0x00` is the root feature.

| Function | Purpose | Params |
|---|---|---|
| `0x01` | `GetProtocolVersion` | none |
| `0x00` | `GetFeature` | 2-byte feature id |

Battery features:

| Id | Name | Read payload |
|---|---|---|
| `0x1004` | unified battery | `[state of charge %, level, charging status, external power]` |
| `0x1000` | battery level status | `[level %, level status, external power status]` |
| `0x1001` | battery voltage | millivolts |

`0x1004` is queried first, then `0x1000`, then `0x1001` (voltage is not converted
to a percentage yet, so it only appears in `--verbose`).

**Assumed:** byte offsets inside the `0x1004` and `0x1000` payloads, and the
meaning of the charging bytes. Both parsers reject values above 100 so a wrong
offset shows up as `unknown` instead of a plausible-looking wrong number.

## Verified vs assumed

| Item | Status |
|---|---|
| Enumerating the receiver's vendor collections | verified |
| Short-report request/response round trip | verified |
| Error frame decoding | verified against two captured frames |
| Slot scan (`0x01`..`0x06`) | verified — all slots answered `0x08` |
| Reading a battery level | **not verified** — no device was awake during development |
| Register `0x02` byte layout | assumed |
| `0x1004` / `0x1000` payload offsets | assumed |
| Bolt (encrypted) receivers | not investigated |

The first run against an awake device should be:

```
battery_status.exe --dump --verbose --raw
```

The `--raw` output contains the exact reply bytes and is enough to correct any
assumption above.
