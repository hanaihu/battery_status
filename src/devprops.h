#pragma once
// Device property keys the project relies on. Most are documented (see
// DEVPKEY_* in devpkey.h); the Bluetooth battery key is not, and is the only
// reason a Windows device can be read without talking to it.
//
// Values verified against hardware on Windows 11 build 26200.

#include "platform.h"

#include <devpropdef.h>  // DEVPROPKEY; not pulled in by windows.h under WIN32_LEAN_AND_MEAN

namespace devprop {

// Battery percentage of a Bluetooth device, DEVPROP_TYPE_BYTE (0..100).
// Undocumented: Windows exposes it for exactly the devices its Settings app shows.
inline constexpr DEVPROPKEY kBattery = { { 0x104EA319, 0x6EE2, 0x4701,
                                           { 0xBD, 0x47, 0x8D, 0xDB, 0xF4, 0x25, 0xBB, 0xE5 } },
                                         2 };

// Whether the Bluetooth link to this devnode is up, DEVPROP_TYPE_BOOLEAN.
inline constexpr DEVPROPKEY kConnected = { { 0x83DA6326, 0x97A6, 0x4088,
                                             { 0x94, 0x53, 0xA1, 0x92, 0x3F, 0x57, 0x3B, 0x29 } },
                                           15 };

// Bluetooth Class of Device, DEVPROP_TYPE_UINT32 (bits 8..12 hold the major class).
inline constexpr DEVPROPKEY kClassOfDevice = { { 0x2BD67D8B, 0x8BEB, 0x48D5,
                                                 { 0x87, 0xE0, 0x6C, 0xDA, 0x34, 0x28, 0x04, 0x0A } },
                                               10 };

// Groups the service nodes of one physical device, DEVPROP_TYPE_GUID.
inline constexpr DEVPROPKEY kContainerId = { { 0x8C7ED206, 0x3F8A, 0x4827,
                                               { 0xB3, 0xAB, 0xAE, 0x9E, 0x1F, 0xAE, 0xFC, 0x6C } },
                                             2 };

// Display name, DEVPROP_TYPE_STRING.
inline constexpr DEVPROPKEY kFriendlyName = { { 0xA45C254E, 0xDF1C, 0x4EFD,
                                                { 0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0 } },
                                              14 };

}  // namespace devprop
