#pragma once
// The one data model every provider fills in. Two rules from docs/QUALITY.md:
// a level is never faked (Unknown is not 0%), and `source` always says which
// code path produced the value.

#include "platform.h"

#include <string>

enum class DeviceKind { Other, Mouse, Keyboard, Headset, Gamepad, Trackpad };

enum class Level {
    Unknown,  // never read
    Stale,    // read before, value kept but no longer confirmed
    Fresh,    // read in the current poll
};

struct DeviceStatus {
    std::wstring name;
    std::wstring key;     // stable identity: "bt:<container>" or "hid:046D:C547"
    std::wstring source;  // "logitech" / "bluetooth" / "hid"
    std::wstring via;     // "2.4 GHz receiver" / "Bluetooth" / "USB"
    DeviceKind kind = DeviceKind::Other;
    Level level = Level::Unknown;
    int percent = -1;
    bool charging = false;
    unsigned short vendorId = 0;
    unsigned short productId = 0;
};

const char* KindName(DeviceKind kind);
const char* LevelName(Level level);

// USB HID vendor ids this project knows something about.
inline constexpr unsigned short kVendorLogitech = 0x046D;
