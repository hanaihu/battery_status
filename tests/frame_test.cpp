// Frame parsing tests. The first four frames were captured from a real
// 046D:C547 receiver on Windows 11 build 26200; the rest are synthetic and
// document the layout we assume.

#include "../src/hidpp.h"

#include <cstdio>

namespace {

int g_failed = 0;

void Check(bool condition, const char* what) {
    if (condition) return;
    std::printf("  FAIL: %s\n", what);
    ++g_failed;
}

void TestParseReply() {
    // Real capture: a slot with no reachable device.
    const unsigned char errorFrame[] = {0x10, 0x01, 0x8F, 0x00, 0x10, 0x08, 0x00};
    const hidpp::Reply error = hidpp::ParseReply(errorFrame, sizeof(errorFrame));
    Check(error.kind == hidpp::ReplyKind::Error, "error frame is an error");
    Check(error.device == 1, "error frame device");
    Check(error.subId == 0x00, "error frame echoes feature index");
    Check(error.address == 0x10, "error frame echoes function byte");
    Check(error.errorCode == 0x08, "error frame code is unknown-device");

    // Real capture: receiver register 0x02.
    const unsigned char registerFrame[] = {0x10, 0xFF, 0x81, 0x02, 0x00, 0x01, 0x00, 0x00};
    const hidpp::Reply reg = hidpp::ParseReply(registerFrame, sizeof(registerFrame));
    Check(reg.kind == hidpp::ReplyKind::Ok, "register frame is ok");
    Check(reg.device == 0xFF, "register frame device");
    Check(reg.subId == 0x81, "register frame sub-id");
    Check(reg.address == 0x02, "register frame register");
    Check(reg.paramCount == 4, "register frame params");
    Check(reg.params[1] == 0x01, "register frame payload preserved");

    Check(hidpp::ParseReply(nullptr, 4).kind == hidpp::ReplyKind::NoReply, "null is no reply");
    Check(hidpp::ParseReply(errorFrame, 2).kind == hidpp::ReplyKind::NoReply, "short frame is no reply");
    Check(hidpp::ParseReply(errorFrame, 4).kind == hidpp::ReplyKind::NoReply, "error without code is no reply");
}

void TestBuildRequest() {
    unsigned char frame[7]{};
    const unsigned char params[] = {0x10, 0x04};
    const int length = hidpp::BuildShortRequest(frame, 0x01, 0x00, hidpp::FunctionByte(0), params, 2);
    Check(length == 7, "short request is 7 bytes");
    Check(frame[0] == 0x10, "report id");
    Check(frame[1] == 0x01, "device index");
    Check(frame[2] == 0x00, "feature index");
    Check(frame[4] == 0x10 && frame[5] == 0x04, "params copied");

    Check(hidpp::FunctionByte(1) == 0x10, "function byte packs function in the high nibble");
    Check(hidpp::FunctionByte(0, 0x0F) == 0x0F, "function byte packs swid in the low nibble");
}

void TestBatteryLevelStatus() {
    const unsigned char discharging[] = {85, 0x00, 0x00, 0x00};
    hidpp::BatteryInfo info = hidpp::ParseBatteryLevelStatus(discharging, sizeof(discharging));
    Check(info.valid && info.percent == 85, "0x1000 level");
    Check(!info.charging, "0x1000 not charging");
    Check(!info.externalPower, "0x1000 no external power");

    const unsigned char plugged[] = {85, 0x00, 0x01, 0x00};
    info = hidpp::ParseBatteryLevelStatus(plugged, sizeof(plugged));
    Check(info.charging && info.externalPower, "0x1000 external power implies charging");

    const unsigned char impossible[] = {200, 0x00, 0x00, 0x00};
    Check(!hidpp::ParseBatteryLevelStatus(impossible, 4).valid, "0x1000 rejects >100");

    Check(!hidpp::ParseBatteryLevelStatus(discharging, 2).valid, "0x1000 rejects short payload");
}

void TestUnifiedBattery() {
    // Real capture from an AA-powered mouse that was at 70% when read.
    // Byte 0 would have meant 15%, byte 1 is the 0..10 level that matches.
    const unsigned char captured[] = {0x0F, 0x07, 0x01, 0x00};
    hidpp::BatteryInfo info = hidpp::ParseUnifiedBattery(captured, sizeof(captured));
    Check(info.valid && info.percent == 70, "0x1004 reads the level byte as 10% steps");
    Check(!info.charging, "0x1004 status bytes are not trusted as charging");

    // A level above 10 cannot be a level, so the state-of-charge byte is used.
    const unsigned char fallback[] = {0x45, 0xFF, 0x00, 0x00};
    info = hidpp::ParseUnifiedBattery(fallback, sizeof(fallback));
    Check(info.valid && info.percent == 0x45, "0x1004 falls back to state of charge");

    const unsigned char impossible[] = {200, 0xFF, 0x00, 0x00};
    Check(!hidpp::ParseUnifiedBattery(impossible, 4).valid, "0x1004 rejects out-of-range payloads");

    Check(!hidpp::ParseUnifiedBattery(captured, 3).valid, "0x1004 rejects short payload");
}

}  // namespace

int main() {
    std::printf("frame tests\n");
    TestParseReply();
    TestBuildRequest();
    TestBatteryLevelStatus();
    TestUnifiedBattery();

    if (g_failed == 0) {
        std::printf("  all passed\n");
        return 0;
    }
    std::printf("  %d failed\n", g_failed);
    return 1;
}
