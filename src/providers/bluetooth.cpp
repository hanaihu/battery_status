#include "../bt.h"
#include "../text.h"
#include "providers.h"

#include <cstdio>
#include <map>
#include <string>

// Bluetooth devices whose level Windows already knows. No radio traffic, so a
// sleeping headset costs nothing to query.

namespace {

bool IsPrimaryNode(const std::wstring& instanceId) {
    return instanceId.compare(0, 12, L"BTHENUM\\DEV_") == 0 || instanceId.compare(0, 10, L"BTHLE\\DEV_") == 0;
}

std::wstring StripServiceSuffix(const std::wstring& name) {
    static const wchar_t* const kSuffixes[] = {
        L" Hands-Free AG Audio",
        L" Stereo",
        L" Avrcp Transport",
        L" Hands-Free",
    };
    for (const wchar_t* suffix : kSuffixes) {
        const size_t length = wcslen(suffix);
        if (name.size() > length && name.compare(name.size() - length, length, suffix) == 0)
            return name.substr(0, name.size() - length);
    }
    return name;
}

struct Group {
    std::wstring name;
    int percent = -1;
    bool connected = false;
    unsigned int classOfDevice = 0;
    bool hasPrimaryNode = false;
};

}  // namespace

void PollBluetooth(PollContext& ctx) {
    std::vector<BluetoothDevice> nodes;
    if (!EnumerateBluetoothDevices(nodes)) return;

    std::map<std::wstring, Group> groups;
    for (const auto& node : nodes) {
        const std::wstring key = node.containerId.empty() ? node.instanceId : node.containerId;
        Group& group = groups[key];

        const bool primary = IsPrimaryNode(node.instanceId);
        if (group.name.empty() || (primary && !group.hasPrimaryNode)) {
            group.name = node.name;
            group.hasPrimaryNode = primary;
        }
        if (node.classOfDevice != 0) group.classOfDevice = node.classOfDevice;
        group.connected = group.connected || node.connected;
        if (node.batteryPercent >= 0 && (group.percent < 0 || node.connected)) group.percent = node.batteryPercent;
    }

    for (const auto& entry : groups) {
        const Group& group = entry.second;
        if (ctx.verbose) {
            std::printf("  bt %s: %s  battery=%d connected=%s\n", ToUtf8(entry.first).c_str(),
                        ToUtf8(group.name).c_str(), group.percent, group.connected ? "yes" : "no");
        }
        if (group.percent < 0) continue;

        // The same physical device may also be reached over a vendor HID path;
        // in that case the vendor driver already reported it.
        bool duplicate = false;
        for (const auto& existing : ctx.devices) duplicate = duplicate || existing.name == group.name;
        if (duplicate) continue;

        DeviceStatus status;
        status.name = StripServiceSuffix(group.name);
        if (status.name.empty()) status.name = L"Bluetooth device";
        status.key = L"bt:" + entry.first;
        status.source = L"bluetooth";
        status.via = L"Bluetooth";
        status.kind = KindFromClassOfDevice(group.classOfDevice);
        status.level = group.connected ? Level::Fresh : Level::Stale;
        status.percent = group.percent;
        ctx.Add(std::move(status), 0, 0);
    }
}
