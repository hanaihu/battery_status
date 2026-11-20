#include "providers.h"

void PollContext::Add(DeviceStatus status, unsigned short vendorId, unsigned short productId) {
    for (const auto& existing : devices)
        if (existing.key == status.key) return;

    if (status.vendorId == 0) status.vendorId = vendorId;
    if (status.productId == 0) status.productId = productId;
    devices.push_back(std::move(status));
    Claim(vendorId, productId);
}

DeviceKind KindFromCollections(const PollContext& ctx, unsigned short vendorId, unsigned short productId) {
    bool mouse = false;
    bool keyboard = false;
    for (const auto& c : ctx.collections) {
        if (c.vendorId != vendorId || c.productId != productId) continue;
        if (c.usagePage != 0x0001) continue;  // Generic Desktop
        if (c.usage == 0x0002) mouse = true;   // Mouse
        if (c.usage == 0x0006) keyboard = true;  // Keyboard
    }
    // Combo receivers expose both, so a product-level guess would be a lie:
    // report "unknown" and let the UI use a generic icon.
    if (mouse && keyboard) return DeviceKind::Other;
    if (keyboard) return DeviceKind::Keyboard;
    if (mouse) return DeviceKind::Mouse;
    return DeviceKind::Other;
}

namespace {

const Provider kProviders[] = {
    {"logitech", PollLogitech},
    {"bluetooth", PollBluetooth},
    {"hid", PollGenericHid},  // fallback, must stay last
};

}  // namespace

const Provider* ProviderTable() { return kProviders; }
int ProviderTableSize() { return static_cast<int>(sizeof(kProviders) / sizeof(kProviders[0])); }
