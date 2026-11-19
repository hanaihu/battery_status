#pragma once
// The provider contract: one function per family of devices, executed in the
// order of the table in table.cpp. A provider may only append to `devices`;
// it never removes or rewrites what an earlier provider found.

#include "../devices.h"
#include "../hid.h"

#include <set>
#include <utility>
#include <vector>

struct PollContext {
    std::vector<HidCollection> collections;
    std::vector<DeviceStatus> devices;
    std::set<std::pair<unsigned short, unsigned short>> claimed;
    bool raw = false;      // print every HID++ frame
    bool verbose = false;  // extra diagnostics

    bool IsClaimed(unsigned short vendorId, unsigned short productId) const {
        return claimed.count({vendorId, productId}) != 0;
    }
    void Claim(unsigned short vendorId, unsigned short productId) { claimed.insert({vendorId, productId}); }

    // Adds a device unless one with the same key is already known.
    void Add(DeviceStatus status, unsigned short vendorId, unsigned short productId);
};

using PollFn = void (*)(PollContext&);

struct Provider {
    const char* name;
    PollFn poll;
};

const Provider* ProviderTable();
int ProviderTableSize();

// Providers, in execution order. The generic HID fallback must stay last.
void PollLogitech(PollContext& ctx);
void PollBluetooth(PollContext& ctx);
void PollGenericHid(PollContext& ctx);

// Device kind guessed from the HID usages the product exposes.
DeviceKind KindFromCollections(const PollContext& ctx, unsigned short vendorId, unsigned short productId);
