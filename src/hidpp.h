#pragma once
// Logitech HID++ frame layout and battery decoding, with no I/O in it.
// Everything here is a pure function over bytes so it can be tested without
// hardware (see tests/frame_test.cpp).

#include "platform.h"

namespace hidpp {

constexpr unsigned char kReportShort = 0x10;  // 7-byte frame
constexpr unsigned char kReportLong = 0x11;   // 20-byte frame
constexpr unsigned char kErrorSubId = 0x8F;   // reply carrying an error code
constexpr unsigned char kReceiverIndex = 0xFF;
constexpr unsigned char kRootFeatureIndex = 0x00;

// Root feature functions.
constexpr unsigned char kRootFuncGetFeature = 0x00;
constexpr unsigned char kRootFuncGetProtocolVersion = 0x01;

// Feature ids (HID++ 2.0 feature table).
constexpr unsigned short kFeatureBatteryLevelStatus = 0x1000;
constexpr unsigned short kFeatureBatteryVoltage = 0x1001;
constexpr unsigned short kFeatureUnifiedBattery = 0x1004;

// HID++ 1.0 sub-ids for register access.
constexpr unsigned char kSubIdReadRegister = 0x81;
constexpr unsigned char kSubIdWriteRegister = 0x80;
constexpr unsigned char kRegisterReceiverConnection = 0x02;

enum class ReplyKind { NoReply, Error, Ok };

struct Reply {
    ReplyKind kind = ReplyKind::NoReply;
    unsigned char device = 0;
    unsigned char subId = 0;    // feature index (2.0) or 0x81 (1.0)
    unsigned char address = 0;  // function byte (2.0) or register (1.0)
    unsigned char params[32]{};
    int paramCount = 0;
    unsigned char errorCode = 0;
};

// Interprets one received report. `length` is the byte count actually read.
Reply ParseReply(const unsigned char* data, int length);

// Writes a 7-byte short request into `out` and returns the frame length.
// `function` is the raw function/swid byte, see FunctionByte().
int BuildShortRequest(unsigned char* out, unsigned char device, unsigned char subId, unsigned char address,
                      const unsigned char* params = nullptr, int paramCount = 0);

inline unsigned char FunctionByte(unsigned char function, unsigned char softwareId = 0) {
    return static_cast<unsigned char>((function << 4) | (softwareId & 0x0F));
}

struct BatteryInfo {
    bool valid = false;
    int percent = -1;
    bool charging = false;
    bool externalPower = false;
};

// 0x1000 function 0: params = [level %, level status, external power status, ...]
BatteryInfo ParseBatteryLevelStatus(const unsigned char* params, int count);

// 0x1004 function 0: params = [state of charge %, battery level, charging status, external power, ...]
BatteryInfo ParseUnifiedBattery(const unsigned char* params, int count);

const char* ErrorName(unsigned char code);

}  // namespace hidpp
