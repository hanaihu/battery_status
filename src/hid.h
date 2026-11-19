#pragma once
// HID enumeration and a minimal overlapped transport. Nothing here knows about
// any vendor protocol; that lives in src/providers/.

#include "platform.h"

#include <string>
#include <vector>

struct HidCollection {
    std::wstring path;
    std::wstring product;
    unsigned short vendorId = 0;
    unsigned short productId = 0;
    unsigned short usagePage = 0;
    unsigned short usage = 0;
    unsigned short inputLength = 0;
    unsigned short outputLength = 0;
    unsigned short featureLength = 0;
};

// Every present top-level HID collection. Returns false only when SetupAPI fails.
bool EnumerateHidCollections(std::vector<HidCollection>& out);

// Owns one device handle. Reads and writes are bounded by a timeout so a device
// that stops answering can never stall the poll thread.
class HidDevice {
public:
    HidDevice() = default;
    ~HidDevice();
    HidDevice(const HidDevice&) = delete;
    HidDevice& operator=(const HidDevice&) = delete;

    bool Open(const std::wstring& path, DWORD access = GENERIC_READ | GENERIC_WRITE);
    void Close();
    bool IsOpen() const { return handle_ != INVALID_HANDLE_VALUE; }

    bool Write(const void* data, DWORD length, DWORD timeoutMs);
    bool Read(void* buffer, DWORD length, DWORD* bytesRead, DWORD timeoutMs);

    // Raw handle, only for HidD_* calls that have no equivalent here.
    HANDLE Native() const { return handle_; }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    OVERLAPPED overlapped_{};
};
