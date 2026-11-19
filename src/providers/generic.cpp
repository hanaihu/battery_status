#include "../text.h"
#include "providers.h"

#include <hidsdi.h>
#include <hidpi.h>

#include <cstdio>
#include <vector>

// The standard HID battery fields, tried on everything no vendor driver claimed:
//   page 0x06 usage 0x20  Battery Strength
//   page 0x85 usage 0x65  Battery System / Relative State of Charge
//
// Collections on the digitizer page are never polled: reading a battery report
// from some pen tablets flips the axes (known upstream bug).

namespace {

constexpr USHORT kPageGenericDeviceControls = 0x06;
constexpr USAGE kUsageBatteryStrength = 0x20;
constexpr USHORT kPageBatterySystem = 0x85;
constexpr USAGE kUsageRelativeStateOfCharge = 0x65;
constexpr USHORT kPageDigitizer = 0x0D;
constexpr USHORT kPagePowerDevice = 0x84;
constexpr USHORT kPageBatterySystemTop = 0x85;

bool IsBatteryUsage(USHORT page, USAGE usage) {
    return (page == kPageGenericDeviceControls && usage == kUsageBatteryStrength) ||
           (page == kPageBatterySystem && usage == kUsageRelativeStateOfCharge);
}

int Normalize(const HIDP_VALUE_CAPS& caps, unsigned long value) {
    if (caps.LogicalMax > caps.LogicalMin) {
        const long span = static_cast<long>(caps.LogicalMax) - static_cast<long>(caps.LogicalMin);
        const long offset = static_cast<long>(value) - static_cast<long>(caps.LogicalMin);
        long percent = span > 0 ? (offset * 100) / span : 0;
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        return static_cast<int>(percent);
    }
    if (value <= 100) return static_cast<int>(value);
    return -1;
}

bool ReadBattery(const HidCollection& collection, int& percent, bool raw) {
    HidDevice device;
    if (!device.Open(collection.path, GENERIC_READ)) return false;

    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(device.Native(), &preparsed)) return false;

    struct Guard {
        PHIDP_PREPARSED_DATA data;
        ~Guard() {
            if (data != nullptr) HidD_FreePreparsedData(data);
        }
    } guard{preparsed};

    const HIDP_REPORT_TYPE types[] = {HidP_Feature, HidP_Input};
    percent = -1;

    for (HIDP_REPORT_TYPE type : types) {
        USHORT count = 0;
        if (HidP_GetValueCaps(type, nullptr, &count, preparsed) != HIDP_STATUS_SUCCESS || count == 0) continue;

        std::vector<HIDP_VALUE_CAPS> caps(count);
        if (HidP_GetValueCaps(type, caps.data(), &count, preparsed) != HIDP_STATUS_SUCCESS) continue;

        const USHORT reportLength =
            type == HidP_Feature ? collection.featureLength : collection.inputLength;
        if (reportLength == 0) continue;

        for (USHORT i = 0; i < count && percent < 0; ++i) {
            const HIDP_VALUE_CAPS& value = caps[i];
            const USAGE usage = value.IsRange ? value.Range.UsageMin : value.NotRange.Usage;
            if (!IsBatteryUsage(value.UsagePage, usage)) continue;

            std::vector<unsigned char> buffer(reportLength, 0);
            buffer[0] = value.ReportID;
            const BOOL ok = type == HidP_Feature ? HidD_GetFeature(device.Native(), buffer.data(), reportLength)
                                                 : HidD_GetInputReport(device.Native(), buffer.data(), reportLength);
            if (!ok) continue;

            unsigned long rawValue = 0;
            if (HidP_GetUsageValue(type, value.UsagePage, value.LinkCollection, usage, &rawValue, preparsed,
                                   reinterpret_cast<PCHAR>(buffer.data()), reportLength) != HIDP_STATUS_SUCCESS)
                continue;

            const int normalized = Normalize(value, rawValue);
            if (raw) {
                std::printf("  hid battery: usage %04X/%04X raw=%lu -> %d%%\n", value.UsagePage, usage, rawValue,
                            normalized);
            }
            percent = normalized;
        }
        if (percent >= 0) break;
    }

    return percent >= 0;
}

}  // namespace

void PollGenericHid(PollContext& ctx) {
    for (const auto& collection : ctx.collections) {
        if (collection.usagePage == kPageDigitizer) continue;
        if (collection.usagePage == kPagePowerDevice || collection.usagePage == kPageBatterySystemTop) continue;
        if (ctx.IsClaimed(collection.vendorId, collection.productId)) continue;
        if (collection.featureLength == 0 && collection.inputLength == 0) continue;

        int percent = -1;
        if (!ReadBattery(collection, percent, ctx.raw)) continue;
        if (percent < 0) continue;

        DeviceStatus status;
        status.name = collection.product.empty() ? L"Wireless device" : collection.product;
        wchar_t key[32]{};
        std::swprintf(key, 32, L"hid:%04X:%04X", collection.vendorId, collection.productId);
        status.key = key;
        status.source = L"hid";
        status.via = L"HID";
        status.kind = KindFromCollections(ctx, collection.vendorId, collection.productId);
        status.level = Level::Fresh;
        status.percent = percent;
        ctx.Add(std::move(status), collection.vendorId, collection.productId);
    }
}
