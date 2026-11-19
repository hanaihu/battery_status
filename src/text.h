#pragma once
// Small text helpers. Console output is UTF-8, so every wide string that reaches
// the terminal goes through ToUtf8() first.

#include "platform.h"

#include <string>

std::string ToUtf8(const std::wstring& text);
std::string ToUtf8(const wchar_t* text);

// "0A 1B 2C" for the first `count` bytes.
std::string HexDump(const unsigned char* data, size_t count);

// "{8C7ED206-3F8A-4827-B3AB-AE9E1FAEFC6C}" — upper case, braces included.
std::wstring GuidToString(const GUID& guid);
