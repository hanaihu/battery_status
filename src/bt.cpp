#include "bt.h"

#include "devprops.h"
#include "text.h"

#include <combaseapi.h>  // StringFromGUID2
#include <setupapi.h>

namespace {

}  // namespace

DeviceKind KindFromClassOfDevice(unsigned int classOfDevice) {
    // Bits 8..12: 1 computer, 2 phone, 4 audio/video, 5 peripheral, ...
    const unsigned int major = (classOfDevice >> 8) & 0x1F;
    if (major == 0x04) return DeviceKind::Headset;   // audio/video (headset, speakers)
    if (major == 0x05) return DeviceKind::Other;     // peripheral: keyboard/mouse share it
    return DeviceKind::Other;
}

bool EnumerateBluetoothDevices(std::vector<BluetoothDevice>& out) {
    HDEVINFO info = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (info == INVALID_HANDLE_VALUE) return false;

    SP_DEVINFO_DATA data{};
    data.cbSize = sizeof(data);

    for (DWORD index = 0; SetupDiEnumDeviceInfo(info, index, &data); ++index) {
        wchar_t instance[512]{};
        if (!SetupDiGetDeviceInstanceIdW(info, &data, instance, 512, nullptr)) continue;

        const bool isBluetooth = wcsncmp(instance, L"BTHENUM\\", 8) == 0 ||
                                 wcsncmp(instance, L"BTHLE\\", 6) == 0 ||
                                 wcsncmp(instance, L"BTHLEDEVICE\\", 12) == 0;
        if (!isBluetooth) continue;

        BluetoothDevice device;
        device.instanceId = instance;

        wchar_t name[512]{};
        DEVPROPKEY friendly = devprop::kFriendlyName;
        if (SetupDiGetDevicePropertyW(info, &data, &friendly, nullptr, reinterpret_cast<PBYTE>(name),
                                      sizeof(name), nullptr, 0))
            device.name = name;

        GUID container{};
        DEVPROPKEY containerKey = devprop::kContainerId;
        if (SetupDiGetDevicePropertyW(info, &data, &containerKey, nullptr, reinterpret_cast<PBYTE>(&container),
                                      sizeof(container), nullptr, 0))
            device.containerId = GuidToString(container);

        DEVPROPKEY batteryKey = devprop::kBattery;
        DEVPROPTYPE type = 0;
        unsigned char percent = 0;
        if (SetupDiGetDevicePropertyW(info, &data, &batteryKey, &type, &percent, sizeof(percent), nullptr, 0) &&
            type == DEVPROP_TYPE_BYTE && percent <= 100)
            device.batteryPercent = percent;

        DEVPROPKEY connectedKey = devprop::kConnected;
        DEVPROP_BOOLEAN connected = 0;
        if (SetupDiGetDevicePropertyW(info, &data, &connectedKey, &type, reinterpret_cast<PBYTE>(&connected),
                                      sizeof(connected), nullptr, 0) &&
            type == DEVPROP_TYPE_BOOLEAN)
            device.connected = connected != 0;

        DEVPROPKEY classKey = devprop::kClassOfDevice;
        unsigned int classOfDevice = 0;
        if (SetupDiGetDevicePropertyW(info, &data, &classKey, &type, reinterpret_cast<PBYTE>(&classOfDevice),
                                      sizeof(classOfDevice), nullptr, 0) &&
            type == DEVPROP_TYPE_UINT32)
            device.classOfDevice = classOfDevice;

        out.push_back(std::move(device));
    }

    SetupDiDestroyDeviceInfoList(info);
    return true;
}
