#include "../hidpp.h"
#include "../text.h"
#include "providers.h"

#include <cstdio>
#include <cwchar>

// Logitech wireless devices behind a Unifying / Nano / Lightspeed receiver.
//
// The receiver answers HID++ itself on 0xFF and forwards to the paired devices
// on indices 1..6. A slot that is empty, or whose device is asleep, answers
// 0x08 UNKNOWN_DEVICE; that is a normal state, not an error worth reporting.

namespace {

constexpr DWORD kWriteTimeoutMs = 250;
constexpr DWORD kReadSliceMs = 200;
constexpr DWORD kReplyBudgetMs = 1500;  // a sleeping device needs the receiver to wake it over RF

constexpr unsigned char kFirstDeviceIndex = 1;
constexpr unsigned char kLastDeviceIndex = 6;

struct Link {
    HidDevice device;
    unsigned short inputLength = 0;
    unsigned short outputLength = 0;
    bool trace = false;

    // Sends one request and waits for the reply that belongs to it. Notifications
    // and replies to older requests are discarded.
    bool Transact(unsigned char dev, unsigned char subId, unsigned char address, const unsigned char* params,
                  int paramCount, hidpp::Reply& reply) {
        unsigned char request[7]{};
        hidpp::BuildShortRequest(request, dev, subId, address, params, paramCount);

        if (trace) {
            std::printf("      > %s\n", HexDump(request, outputLength).c_str());
        }
        if (!device.Write(request, outputLength, kWriteTimeoutMs)) return false;

        const unsigned long long deadline = GetTickCount64() + kReplyBudgetMs;
        bool announcedTimeout = false;
        while (GetTickCount64() < deadline) {
            unsigned char buffer[64]{};
            DWORD received = 0;
            const unsigned long long remaining = deadline - GetTickCount64();
            const DWORD slice = remaining < kReadSliceMs ? static_cast<DWORD>(remaining) : kReadSliceMs;
            if (!device.Read(buffer, inputLength, &received, slice)) {
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
            const bool matched = candidate.kind != hidpp::ReplyKind::NoReply && candidate.device == dev &&
                                 (candidate.subId == subId || candidate.address == subId);
            if (trace) {
                std::printf("      < %s  %s\n", HexDump(buffer, received).c_str(), matched ? "" : "(not for us)");
            }
            if (!matched) continue;

            reply = candidate;
            return true;
        }
        if (trace) std::printf("      (no reply within %u ms)\n", kReplyBudgetMs);
        return false;
    }
};

// Picks the vendor collection that carries HID++ for this product.
const HidCollection* FindHidppCollection(const PollContext& ctx, unsigned short vendorId,
                                         unsigned short productId) {
    const HidCollection* shortReport = nullptr;
    const HidCollection* longReport = nullptr;
    for (const auto& c : ctx.collections) {
        if (c.vendorId != vendorId || c.productId != productId) continue;
        if (c.usagePage != 0xFF00) continue;
        if (c.outputLength == 0) continue;
        if (c.outputLength <= 8) {
            if (shortReport == nullptr) shortReport = &c;
        } else if (longReport == nullptr) {
            longReport = &c;
        }
    }
    return shortReport != nullptr ? shortReport : longReport;
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

void PrintReceiverDiagnostics(Link& link, PollContext& ctx) {
    hidpp::Reply reply;
    if (!link.Transact(hidpp::kReceiverIndex, hidpp::kSubIdReadRegister, hidpp::kRegisterReceiverConnection, nullptr,
                       0, reply)) {
        std::printf("  receiver register 0x02: no reply\n");
        return;
    }
    if (reply.kind == hidpp::ReplyKind::Error) {
        std::printf("  receiver register 0x02: error 0x%02X (%s)\n", reply.errorCode, hidpp::ErrorName(reply.errorCode));
        return;
    }
    std::printf("  receiver register 0x02: %s\n", HexDump(reply.params, reply.paramCount).c_str());
    if (ctx.verbose) std::printf("    (byte layout not fully documented; see docs/PROTOCOL.md)\n");
}

void PollOneProduct(PollContext& ctx, unsigned short vendorId, unsigned short productId) {
    const HidCollection* collection = FindHidppCollection(ctx, vendorId, productId);
    if (collection == nullptr) return;

    Link link;
    link.inputLength = collection->inputLength;
    link.outputLength = collection->outputLength;
    link.trace = ctx.raw;

    if (!link.device.Open(collection->path)) {
        if (ctx.verbose) std::printf("  cannot open %s\n", ToUtf8(collection->product).c_str());
        return;
    }
    if (collection->outputLength > 8 && ctx.verbose) {
        std::printf("  only a long-report collection is available; using it\n");
    }

    if (ctx.verbose) PrintReceiverDiagnostics(link, ctx);

    for (unsigned char index = kFirstDeviceIndex; index <= kLastDeviceIndex; ++index) {
        hidpp::Reply reply;
        if (!link.Transact(index, hidpp::kRootFeatureIndex,
                           hidpp::FunctionByte(hidpp::kRootFuncGetProtocolVersion), nullptr, 0, reply)) {
            continue;
        }
        if (reply.kind == hidpp::ReplyKind::Error) {
            if (ctx.verbose) {
                std::printf("  slot %u: error 0x%02X (%s)\n", index, reply.errorCode,
                            hidpp::ErrorName(reply.errorCode));
            }
            continue;
        }

        const unsigned char major = reply.paramCount > 0 ? reply.params[0] : 0;
        const unsigned char minor = reply.paramCount > 1 ? reply.params[1] : 0;
        if (ctx.verbose) std::printf("  slot %u: HID++ %u.%u\n", index, major, minor);

        hidpp::BatteryInfo battery;
        const unsigned short features[] = {hidpp::kFeatureUnifiedBattery, hidpp::kFeatureBatteryLevelStatus,
                                          hidpp::kFeatureBatteryVoltage};
        for (unsigned short featureId : features) {
            unsigned char featureIndex = 0;
            if (!QueryFeatureIndex(link, index, featureId, featureIndex, reply)) {
                if (ctx.verbose && reply.kind == hidpp::ReplyKind::Error)
                    std::printf("  feature 0x%04X: error 0x%02X\n", featureId, reply.errorCode);
                continue;
            }

            if (!link.Transact(index, featureIndex, hidpp::FunctionByte(0), nullptr, 0, reply)) continue;
            if (reply.kind != hidpp::ReplyKind::Ok) continue;

            if (ctx.verbose || ctx.raw) {
                std::printf("  feature 0x%04X -> %s\n", featureId, HexDump(reply.params, reply.paramCount).c_str());
            }

            battery = featureId == hidpp::kFeatureUnifiedBattery
                          ? hidpp::ParseUnifiedBattery(reply.params, reply.paramCount)
                          : hidpp::ParseBatteryLevelStatus(reply.params, reply.paramCount);
            if (battery.valid) break;
        }

        DeviceStatus status;
        status.name = collection->product.empty() ? L"Logitech wireless device" : collection->product;
        status.source = L"logitech";
        status.via = L"2.4 GHz receiver";
        status.vendorId = vendorId;
        status.productId = productId;
        status.kind = KindFromCollections(ctx, vendorId, productId);

        wchar_t key[32]{};
        std::swprintf(key, 32, L"hid:%04X:%04X", vendorId, productId);
        status.key = key;

        if (battery.valid) {
            status.level = Level::Fresh;
            status.percent = battery.percent;
            status.charging = battery.charging;
        } else {
            status.level = Level::Unknown;
        }
        ctx.Add(std::move(status), vendorId, productId);
        return;  // one device per HID++ product
    }

    if (ctx.verbose) {
        // The receiver is a transport, not a device: with no reachable slot it
        // must not appear in the device list at all.
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
