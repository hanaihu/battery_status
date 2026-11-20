#include "../hidpp.h"
#include "../text.h"
#include "providers.h"

#include <cstdio>
#include <cwchar>

// Logitech wireless devices behind a Unifying / Nano / Lightspeed receiver.
//
// Two generations have to be handled:
//   * HID++ 2.0 devices expose battery through feature discovery (0x1004 / 0x1000).
//   * HID++ 1.0 devices only have registers, battery lives in 0x07 / 0x0D.
// A 2.0 ping that answers 0x08 proves the slot is empty. A ping that gets no
// reply at all proves nothing, so the register path is still tried.

namespace {

constexpr DWORD kWriteTimeoutMs = 250;
constexpr DWORD kReadSliceMs = 200;
constexpr DWORD kReplyBudgetMs = 1500;  // a sleeping device needs the receiver to wake it over RF

constexpr unsigned char kFirstDeviceIndex = 1;
constexpr unsigned char kLastDeviceIndex = 6;

// HID++ 1.0 device registers.
constexpr unsigned char kRegisterBatteryStatus = 0x07;
constexpr unsigned char kRegisterBatteryCharge = 0x0D;

// Status nibble of register 0x0D, byte 2.
constexpr unsigned char kChargeDischarging = 0x30;
constexpr unsigned char kChargeRecharging = 0x50;
constexpr unsigned char kChargeFull = 0x90;

struct Link {
    HidDevice shortDevice;
    HidDevice longDevice;
    unsigned short shortInput = 0;
    unsigned short shortOutput = 0;
    unsigned short longInput = 0;
    unsigned short longOutput = 0;
    bool trace = false;

    // Discards anything already queued. Without this, a late reply to the
    // previous request is matched against the next one.
    void Drain(HidDevice& device, unsigned short inLength) {
        for (int i = 0; i < 8; ++i) {
            unsigned char buffer[64]{};
            DWORD received = 0;
            if (!device.Read(buffer, inLength, &received, 1)) return;
            if (received == 0) return;
            if (trace) std::printf("      (drained) %s\n", HexDump(buffer, received).c_str());
        }
    }

    bool Try(HidDevice& device, unsigned short outLength, unsigned short inLength, unsigned char reportId,
             unsigned char dev, unsigned char subId, unsigned char address, const unsigned char* params,
             int paramCount, hidpp::Reply& reply) {
        unsigned char request[32]{};
        request[0] = reportId;
        request[1] = dev;
        request[2] = subId;
        request[3] = address;
        for (int i = 0; i < paramCount && (4 + i) < outLength; ++i) request[4 + i] = params[i];

        Drain(device, inLength);
        if (trace) std::printf("      > %s\n", HexDump(request, outLength).c_str());
        if (!device.Write(request, outLength, kWriteTimeoutMs)) return false;

        const unsigned long long deadline = GetTickCount64() + kReplyBudgetMs;
        bool announcedTimeout = false;
        while (GetTickCount64() < deadline) {
            unsigned char buffer[64]{};
            DWORD received = 0;
            const unsigned long long remaining = deadline - GetTickCount64();
            const DWORD slice = remaining < kReadSliceMs ? static_cast<DWORD>(remaining) : kReadSliceMs;
            if (!device.Read(buffer, inLength, &received, slice)) {
                if (trace && !announcedTimeout) {
                    std::printf("      (waiting for the receiver)\n");
                    announcedTimeout = true;
                }
                continue;
            }
            if (received < 6) {
                if (trace) std::printf("      < %s  (too short)\n", HexDump(buffer, received).c_str());
                continue;
            }

            const hidpp::Reply candidate = hidpp::ParseReply(buffer, static_cast<int>(received));
            // Error frames echo the request's sub-id and address, so one strict
            // comparison covers both kinds of reply.
            const bool matched = candidate.kind != hidpp::ReplyKind::NoReply && candidate.device == dev &&
                                 candidate.subId == subId && candidate.address == address;
            if (trace) {
                std::printf("      < %s  %s\n", HexDump(buffer, received).c_str(), matched ? "" : "(not for us)");
            }
            if (!matched) continue;

            reply = candidate;
            return true;
        }
        if (trace) std::printf("      (no reply within %u ms)\n", static_cast<unsigned>(kReplyBudgetMs));
        return false;
    }

    // Short report first (7 bytes, what most receivers accept), then the 20-byte
    // long report: some HID++ 2.0 devices only answer the long form.
    bool Transact(unsigned char dev, unsigned char subId, unsigned char address, const unsigned char* params,
                  int paramCount, hidpp::Reply& reply) {
        if (shortDevice.IsOpen() &&
            Try(shortDevice, shortOutput, shortInput, 0x10, dev, subId, address, params, paramCount, reply))
            return true;
        if (longDevice.IsOpen() &&
            Try(longDevice, longOutput, longInput, 0x11, dev, subId, address, params, paramCount, reply))
            return true;
        return false;
    }
};

void FindHidppCollections(const PollContext& ctx, unsigned short vendorId, unsigned short productId,
                         const HidCollection** shortReport, const HidCollection** longReport) {
    *shortReport = nullptr;
    *longReport = nullptr;
    for (const auto& c : ctx.collections) {
        if (c.vendorId != vendorId || c.productId != productId) continue;
        if (c.usagePage != 0xFF00) continue;
        if (c.outputLength == 0) continue;
        if (c.outputLength <= 8) {
            if (*shortReport == nullptr) *shortReport = &c;
        } else if (*longReport == nullptr) {
            *longReport = &c;
        }
    }
}

// Asks the receiver to ping its paired devices, which is what the official
// software does at startup. Harmless when already connected.
void WakeLinks(Link& link) {
    const unsigned char enable = 0x02;
    hidpp::Reply reply;
    link.Transact(hidpp::kReceiverIndex, hidpp::kSubIdWriteRegister, hidpp::kRegisterReceiverConnection, &enable, 1,
                  reply);
}

bool QueryFeatureIndex(Link& link, unsigned char dev, unsigned short featureId, unsigned char& featureIndex,
                       hidpp::Reply& reply) {
    const unsigned char params[2] = {static_cast<unsigned char>(featureId >> 8),
                                     static_cast<unsigned char>(featureId & 0xFF)};
    if (!link.Transact(dev, hidpp::kRootFeatureIndex, hidpp::FunctionByte(hidpp::kRootFuncGetFeature), params, 2,
                       reply))
        return false;
    if (reply.kind != hidpp::ReplyKind::Ok || reply.paramCount < 1) return false;
    featureIndex = reply.params[0];
    return featureIndex != 0;
}

// HID++ 2.0 path. Returns true only when a level was actually read.
bool ReadBattery2_0(Link& link, unsigned char index, hidpp::BatteryInfo& battery, bool crossCheck) {
    const unsigned short features[] = {hidpp::kFeatureUnifiedBattery, hidpp::kFeatureBatteryLevelStatus};
    for (unsigned short featureId : features) {
        hidpp::Reply reply;
        unsigned char featureIndex = 0;
        if (!QueryFeatureIndex(link, index, featureId, featureIndex, reply)) continue;
        if (!link.Transact(index, featureIndex, hidpp::FunctionByte(0), nullptr, 0, reply)) continue;
        if (reply.kind != hidpp::ReplyKind::Ok) continue;

        if (link.trace) std::printf("    feature 0x%04X -> %s\n", featureId, HexDump(reply.params, reply.paramCount).c_str());

        const hidpp::BatteryInfo parsed = featureId == hidpp::kFeatureUnifiedBattery
                                              ? hidpp::ParseUnifiedBattery(reply.params, reply.paramCount)
                                              : hidpp::ParseBatteryLevelStatus(reply.params, reply.paramCount);
        if (!parsed.valid) continue;
        battery = parsed;

        // The two features report the same battery in different shapes. Reading
        // both makes a wrong byte offset obvious instead of plausible.
        if (crossCheck && featureId == hidpp::kFeatureUnifiedBattery) {
            hidpp::Reply other;
            unsigned char otherIndex = 0;
            if (QueryFeatureIndex(link, index, hidpp::kFeatureBatteryLevelStatus, otherIndex, other) &&
                link.Transact(index, otherIndex, hidpp::FunctionByte(0), nullptr, 0, other) &&
                other.kind == hidpp::ReplyKind::Ok)
                std::printf("    cross-check 0x1000 -> %s\n", HexDump(other.params, other.paramCount).c_str());
        }
        return true;
    }
    return false;
}

// HID++ 1.0 path, for devices that predate feature discovery.
// Register 0x0D: [charge %, level status, charging status, ...]
bool ReadBattery1_0(Link& link, unsigned char index, hidpp::BatteryInfo& battery) {
    hidpp::Reply reply;
    if (!link.Transact(index, hidpp::kSubIdReadRegister, kRegisterBatteryCharge, nullptr, 0, reply)) return false;
    if (reply.kind != hidpp::ReplyKind::Ok || reply.paramCount < 3) {
        if (link.trace && reply.kind == hidpp::ReplyKind::Error)
            std::printf("    reg 0x0D: error 0x%02X (%s)\n", reply.errorCode, hidpp::ErrorName(reply.errorCode));
        return false;
    }
    if (link.trace) std::printf("    reg 0x0D -> %s\n", HexDump(reply.params, reply.paramCount).c_str());

    const int percent = reply.params[0];
    if (percent > 100) return false;

    const unsigned char status = reply.params[2] & 0xF0;
    battery.valid = true;
    battery.percent = percent;
    battery.charging = status == kChargeRecharging || status == kChargeFull;
    battery.externalPower = status != kChargeDischarging;
    return true;
}

void PollOneProduct(PollContext& ctx, unsigned short vendorId, unsigned short productId) {
    const HidCollection* shortCollection = nullptr;
    const HidCollection* longCollection = nullptr;
    FindHidppCollections(ctx, vendorId, productId, &shortCollection, &longCollection);
    if (shortCollection == nullptr && longCollection == nullptr) return;

    Link link;
    if (shortCollection != nullptr) {
        link.shortInput = shortCollection->inputLength;
        link.shortOutput = shortCollection->outputLength;
        link.shortDevice.Open(shortCollection->path);
    }
    if (longCollection != nullptr) {
        link.longInput = longCollection->inputLength;
        link.longOutput = longCollection->outputLength;
        link.longDevice.Open(longCollection->path);
    }
    link.trace = ctx.raw;

    if (!link.shortDevice.IsOpen() && !link.longDevice.IsOpen()) {
        if (ctx.verbose) std::printf("  cannot open any HID++ collection for %04X:%04X\n", vendorId, productId);
        return;
    }

    WakeLinks(link);

    for (unsigned char index = kFirstDeviceIndex; index <= kLastDeviceIndex; ++index) {
        hidpp::Reply reply;
        bool answered = link.Transact(index, hidpp::kRootFeatureIndex,
                                      hidpp::FunctionByte(hidpp::kRootFuncGetProtocolVersion), nullptr, 0, reply);
        if (answered && reply.kind == hidpp::ReplyKind::Error && reply.errorCode == 0x08) {
            if (ctx.verbose) std::printf("  slot %u: empty (0x08)\n", index);
            continue;  // a definite "no device here", so the register path would fail too
        }
        if (answered && reply.kind == hidpp::ReplyKind::Ok && ctx.verbose) {
            const unsigned char major = reply.paramCount > 0 ? reply.params[0] : 0;
            const unsigned char minor = reply.paramCount > 1 ? reply.params[1] : 0;
            std::printf("  slot %u: HID++ %u.%u\n", index, major, minor);
        }

        hidpp::BatteryInfo battery;
        if (!ReadBattery2_0(link, index, battery, ctx.verbose)) ReadBattery1_0(link, index, battery);
        if (!battery.valid) {
            if (ctx.verbose) std::printf("  slot %u: no battery available\n", index);
            continue;
        }

        DeviceStatus status;
        const HidCollection* source = shortCollection != nullptr ? shortCollection : longCollection;
        status.name = source->product.empty() ? L"Logitech wireless device" : source->product;
        wchar_t key[32]{};
        std::swprintf(key, 32, L"hid:%04X:%04X", vendorId, productId);
        status.key = key;
        status.source = L"logitech";
        status.via = L"2.4 GHz receiver";
        status.vendorId = vendorId;
        status.productId = productId;
        status.kind = KindFromCollections(ctx, vendorId, productId);
        status.level = Level::Fresh;
        status.percent = battery.percent;
        status.charging = battery.charging;
        ctx.Add(std::move(status), vendorId, productId);
        return;  // one device per HID++ product
    }

    if (ctx.verbose) {
        std::printf("  %04X:%04X: no device answered on slots %u..%u\n", vendorId, productId, kFirstDeviceIndex,
                    kLastDeviceIndex);
    }
}

}  // namespace

void PollLogitech(PollContext& ctx) {
    std::set<std::pair<unsigned short, unsigned short>> products;
    for (const auto& c : ctx.collections) {
        if (c.vendorId == kVendorLogitech) products.insert({c.vendorId, c.productId});
    }
    for (const auto& product : products) {
        if (ctx.IsClaimed(product.first, product.second)) continue;
        PollOneProduct(ctx, product.first, product.second);
    }
}
