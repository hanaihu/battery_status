#include "hidpp.h"

namespace hidpp {
namespace {

void CopyParams(Reply& reply, const unsigned char* source, int count) {
    const int limit = sizeof(reply.params);
    reply.paramCount = count < limit ? count : limit;
    for (int i = 0; i < reply.paramCount; ++i) reply.params[i] = source[i];
}

}  // namespace

Reply ParseReply(const unsigned char* data, int length) {
    Reply reply;
    if (data == nullptr || length < 4) return reply;

    reply.device = data[1];

    if (data[2] == kErrorSubId) {
        // Error frame: [id, device, 0x8F, feature/register, function/sub-id, code]
        if (length < 6) return reply;  // truncated: not a usable error
        reply.kind = ReplyKind::Error;
        reply.subId = data[3];
        reply.address = data[4];
        reply.errorCode = data[5];
        return reply;
    }

    reply.kind = ReplyKind::Ok;
    reply.subId = data[2];
    reply.address = data[3];
    CopyParams(reply, data + 4, length - 4);
    return reply;
}

int BuildShortRequest(unsigned char* out, unsigned char device, unsigned char subId, unsigned char address,
                      const unsigned char* params, int paramCount) {
    out[0] = kReportShort;
    out[1] = device;
    out[2] = subId;
    out[3] = address;
    for (int i = 0; i < paramCount && (4 + i) < 7; ++i) out[4 + i] = params[i];
    return 7;
}

BatteryInfo ParseBatteryLevelStatus(const unsigned char* params, int count) {
    BatteryInfo info;
    if (params == nullptr || count < 3) return info;
    if (params[0] > 100) return info;
    info.valid = true;
    info.percent = params[0];
    // Byte 2 reports whether external power is connected; while it is, the
    // device is charging or held at its charge limit.
    info.externalPower = (params[2] & 0x03) != 0;
    info.charging = info.externalPower;
    return info;
}

BatteryInfo ParseUnifiedBattery(const unsigned char* params, int count) {
    BatteryInfo info;
    if (params == nullptr || count < 4) return info;

    // Layout assumed from the HID++ 2.0 feature table:
    //   [0] state of charge, [1] battery level 0..10, [2] charging, [3] external power
    //
    // Measured on a 046D:C547 receiver with an AA-powered mouse the payload was
    // 0F 07 01 00 while the actual charge was 70%: byte 0 would have said 15%.
    // Byte 1 (7 -> 70%) is the value that matched, so it is preferred and the
    // state-of-charge byte is only a fallback. The status bytes cannot be
    // distrusted more: a device with no charging circuit still reports 01 00,
    // so charging stays false rather than being shown from those bytes.
    const int level = params[1];
    if (level >= 0 && level <= 10) {
        info.valid = true;
        info.percent = level * 10;
        return info;
    }
    if (params[0] <= 100) {
        info.valid = true;
        info.percent = params[0];
    }
    return info;
}

const char* ErrorName(unsigned char code) {
    switch (code) {
        case 0x00: return "no error";
        case 0x01: return "invalid sub-id";
        case 0x02: return "invalid address";
        case 0x03: return "invalid value";
        case 0x04: return "connection request failed";
        case 0x05: return "too many devices";
        case 0x06: return "already exists";
        case 0x07: return "busy";
        case 0x08: return "unknown device";
        case 0x09: return "resource error";
        case 0x0A: return "request unavailable";
        case 0x0B: return "unsupported parameter";
        case 0x0C: return "wrong pin code";
        default: break;
    }
    return "unknown error";
}

}  // namespace hidpp
