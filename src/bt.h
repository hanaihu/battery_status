#pragma once
// Bluetooth devices as Windows itself sees them. The battery value here is the
// one the Settings app shows, so it needs no pairing work from us and no radio
// traffic — which also means it never drains the device.

#include "devices.h"
#include "platform.h"

#include <string>
#include <vector>

struct BluetoothDevice {
    std::wstring instanceId;
    std::wstring containerId;
    std::wstring name;
    int batteryPercent = -1;  // -1 when the devnode has no battery property
    bool connected = false;
    unsigned int classOfDevice = 0;
};

// Every present BTHENUM / BTHLE / BTHLEDEVICE node. Returns false only when
// SetupAPI fails.
bool EnumerateBluetoothDevices(std::vector<BluetoothDevice>& out);

// Major service class bits (8..12) of a Class of Device value.
DeviceKind KindFromClassOfDevice(unsigned int classOfDevice);
