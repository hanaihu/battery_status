#include "devices.h"

const char* KindName(DeviceKind kind) {
    switch (kind) {
        case DeviceKind::Mouse: return "mouse";
        case DeviceKind::Keyboard: return "keyboard";
        case DeviceKind::Headset: return "headset";
        case DeviceKind::Gamepad: return "gamepad";
        case DeviceKind::Trackpad: return "trackpad";
        case DeviceKind::Other: break;
    }
    return "device";
}

const char* LevelName(Level level) {
    switch (level) {
        case Level::Fresh: return "fresh";
        case Level::Stale: return "stale";
        case Level::Unknown: break;
    }
    return "unknown";
}
