#include "hid.h"

#include "text.h"

#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

#include <vector>

namespace {

// Reads a property string without allocating more than the buffer we hand over.
std::wstring ProductString(HANDLE device) {
    wchar_t buffer[256]{};
    if (!HidD_GetProductString(device, buffer, sizeof(buffer))) return {};
    return buffer;
}

bool WaitFor(HANDLE handle, OVERLAPPED& overlapped, DWORD timeoutMs, DWORD* transferred) {
    if (WaitForSingleObject(overlapped.hEvent, timeoutMs) != WAIT_OBJECT_0) {
        CancelIo(handle);
        return false;
    }
    return GetOverlappedResult(handle, &overlapped, transferred, FALSE) != 0;
}

}  // namespace

bool EnumerateHidCollections(std::vector<HidCollection>& out) {
    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO info = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) return false;

    SP_DEVICE_INTERFACE_DATA interfaceData{};
    interfaceData.cbSize = sizeof(interfaceData);

    for (DWORD index = 0; SetupDiEnumDeviceInterfaces(info, nullptr, &hidGuid, index, &interfaceData); ++index) {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(info, &interfaceData, nullptr, 0, &needed, nullptr);
        if (needed == 0) continue;

        std::vector<unsigned char> buffer(needed);
        auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(info, &interfaceData, detail, needed, nullptr, nullptr)) continue;

        HANDLE device = CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, 0, nullptr);
        if (device == INVALID_HANDLE_VALUE) continue;

        HidCollection collection;
        HIDD_ATTRIBUTES attributes{};
        attributes.Size = sizeof(attributes);
        bool keep = false;

        if (HidD_GetAttributes(device, &attributes)) {
            PHIDP_PREPARSED_DATA preparsed = nullptr;
            HIDP_CAPS caps{};
            if (HidD_GetPreparsedData(device, &preparsed) && HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS) {
                collection.vendorId = attributes.VendorID;
                collection.productId = attributes.ProductID;
                collection.usagePage = caps.UsagePage;
                collection.usage = caps.Usage;
                collection.inputLength = caps.InputReportByteLength;
                collection.outputLength = caps.OutputReportByteLength;
                collection.featureLength = caps.FeatureReportByteLength;
                collection.path = detail->DevicePath;
                collection.product = ProductString(device);
                keep = true;
            }
            if (preparsed != nullptr) HidD_FreePreparsedData(preparsed);
        }

        CloseHandle(device);
        if (keep) out.push_back(std::move(collection));
    }

    SetupDiDestroyDeviceInfoList(info);
    return true;
}

HidDevice::~HidDevice() { Close(); }

bool HidDevice::Open(const std::wstring& path, DWORD access) {
    Close();
    handle_ = CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                          FILE_FLAG_OVERLAPPED, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) return false;
    overlapped_ = OVERLAPPED{};
    overlapped_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (overlapped_.hEvent == nullptr) {
        Close();
        return false;
    }
    return true;
}

void HidDevice::Close() {
    if (overlapped_.hEvent != nullptr) {
        CloseHandle(overlapped_.hEvent);
        overlapped_.hEvent = nullptr;
    }
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}

bool HidDevice::Write(const void* data, DWORD length, DWORD timeoutMs) {
    if (!IsOpen()) return false;
    ResetEvent(overlapped_.hEvent);
    DWORD written = 0;
    if (WriteFile(handle_, data, length, nullptr, &overlapped_)) return true;
    if (GetLastError() != ERROR_IO_PENDING) return false;
    return WaitFor(handle_, overlapped_, timeoutMs, &written);
}

bool HidDevice::Read(void* buffer, DWORD length, DWORD* bytesRead, DWORD timeoutMs) {
    if (!IsOpen()) return false;
    ResetEvent(overlapped_.hEvent);
    DWORD received = 0;
    if (ReadFile(handle_, buffer, length, nullptr, &overlapped_)) {
        // Completed synchronously: the result is already available.
        GetOverlappedResult(handle_, &overlapped_, &received, FALSE);
        if (bytesRead != nullptr) *bytesRead = received;
        return true;
    }
    if (GetLastError() != ERROR_IO_PENDING) return false;
    if (!WaitFor(handle_, overlapped_, timeoutMs, &received)) return false;
    if (bytesRead != nullptr) *bytesRead = received;
    return true;
}
