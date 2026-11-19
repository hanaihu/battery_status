// Battery Status — entry point.
//
// The tray UI is not implemented yet, so every run currently behaves like a
// diagnostic tool: --dump prints what each provider can see on this machine.

#include "devices.h"
#include "hid.h"
#include "providers/providers.h"
#include "text.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kVersion = "0.1.0";

void PrintUsage() {
    std::printf(
        "Battery Status %s\n"
        "\n"
        "  battery_status --dump [--verbose|--raw]\n"
        "\n"
        "    --dump      print every device a provider can see, with its battery\n"
        "    --verbose   provider diagnostics (slots, reconnect attempts, reasons)\n"
        "    --raw       print every HID++ frame sent and received\n"
        "\n"
        "  battery_status --version\n",
        kVersion);
}

void PrintCollections(const PollContext& ctx, bool verbose) {
    std::printf("HID collections: %zu\n", ctx.collections.size());
    if (!verbose) return;
    for (const auto& c : ctx.collections) {
        std::printf("  %04X/%04X  %04X:%04X  in %-3u out %-3u  %s\n", c.usagePage, c.usage, c.vendorId, c.productId,
                    c.inputLength, c.outputLength, ToUtf8(c.product).c_str());
    }
}

void PrintDevices(const PollContext& ctx) {
    std::printf("\nDevices: %zu\n", ctx.devices.size());
    if (ctx.devices.empty()) {
        std::printf("  (no provider found a battery level)\n");
        return;
    }

    std::printf("  %-7s %-5s %-9s %-20s %s\n", "state", "level", "kind", "name", "via / source");
    for (const auto& d : ctx.devices) {
        char level[16] = "--";
        if (d.percent >= 0) std::snprintf(level, sizeof(level), "%d%%", d.percent);
        std::printf("  %-7s %-5s %-9s %-20s %s%s / %s\n", LevelName(d.level), level, KindName(d.kind),
                    ToUtf8(d.name).c_str(), ToUtf8(d.via).c_str(), d.charging ? " (charging)" : "",
                    ToUtf8(d.source).c_str());
    }
}

int Dump(bool verbose, bool raw) {
    std::printf("Battery Status %s\n\n", kVersion);

    PollContext ctx;
    ctx.raw = raw;
    ctx.verbose = verbose;
    if (!EnumerateHidCollections(ctx.collections)) std::printf("HID enumeration failed\n");
    PrintCollections(ctx, verbose);
    std::printf("\n");

    const Provider* providers = ProviderTable();
    for (int i = 0; i < ProviderTableSize(); ++i) {
        if (verbose) std::printf("provider: %s\n", providers[i].name);
        providers[i].poll(ctx);
    }

    PrintDevices(ctx);
    if (!verbose && !raw) std::printf("\nRun with --verbose for provider diagnostics, --raw for HID++ frames.\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);

    bool verbose = false;
    bool raw = false;
    bool dump = false;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--dump") == 0) {
            dump = true;
        } else if (std::strcmp(arg, "--verbose") == 0) {
            verbose = true;
        } else if (std::strcmp(arg, "--raw") == 0) {
            raw = true;
        } else if (std::strcmp(arg, "--version") == 0) {
            std::printf("Battery Status %s\n", kVersion);
            return 0;
        } else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            PrintUsage();
            return 0;
        } else {
            std::printf("unknown option: %s\n\n", arg);
            PrintUsage();
            return 2;
        }
    }

    if (!dump) {
        PrintUsage();
        std::printf("\nThe tray icon is not implemented yet; use --dump.\n");
        return 0;
    }
    return Dump(verbose, raw);
}
