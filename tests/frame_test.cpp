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
    const unsigned char idle[] = {72, 0x06, 0x00, 0x00};
    hidpp::BatteryInfo info = hidpp::ParseUnifiedBattery(idle, sizeof(idle));
    Check(info.valid && info.percent == 72, "0x1004 level");
    Check(!info.charging, "0x1004 discharging");
    Check(!info.externalPower, "0x1004 no external power");

    const unsigned char charging[] = {72, 0x06, 0x01, 0x01};
    info = hidpp::ParseUnifiedBattery(charging, sizeof(charging));
    Check(info.charging && info.externalPower, "0x1004 charging");

    const unsigned char complete[] = {100, 0x0A, 0x02, 0x01};
    info = hidpp::ParseUnifiedBattery(complete, sizeof(complete));
    Check(info.valid && info.percent == 100 && info.charging, "0x1004 full");

    Check(!hidpp::ParseUnifiedBattery(idle, 3).valid, "0x1004 rejects short payload");
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
