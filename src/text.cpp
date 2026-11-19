#include "text.h"

#include "platform.h"

#include <cstdio>
#include <cwchar>

std::string ToUtf8(const wchar_t* text) {
    if (text == nullptr || *text == L'\0') return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &out[0], needed, nullptr, nullptr);
    return out;
}

std::string ToUtf8(const std::wstring& text) { return ToUtf8(text.c_str()); }

std::string HexDump(const unsigned char* data, size_t count) {
    std::string out;
    out.reserve(count * 3);
    char byte[4];
    for (size_t i = 0; i < count; ++i) {
        std::snprintf(byte, sizeof(byte), "%02X ", data[i]);
        out += byte;
    }
    if (!out.empty()) out.pop_back();
    return out;
}

std::wstring GuidToString(const GUID& guid) {
    wchar_t buffer[64]{};
    std::swprintf(buffer, 64, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", guid.Data1, guid.Data2,
                  guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4],
                  guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return buffer;
}
