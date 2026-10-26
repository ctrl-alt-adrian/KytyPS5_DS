// Pad input as the game sees it (src/libs/controller.cpp): the state history scePadRead gives.
#include "common/emulatorConfig.h"
#include "common/logging/log.h"
#include "common/subsystems.h"
#include "common/threads.h"
#include "libs/controller.h"
#include "libs/padData.h"

#include <cstdio>
#include <cstdlib>

namespace {

using namespace Libs::Controller;

void Check(bool value, const char *text) {
  if (!value) {
    std::fprintf(stderr, "PadInputTests: failed: %s\n", text);
    std::abort();
  }
}

// The unread states, oldest first (Demon's Souls reads 64 a tick).
int Read(PadData *data) { return PadRead(1, data, 64); }

// A tap, then far more stick events before the next read than the history holds: the read still has the press and
// the release, and its newest state the sticks' last values (KYTY_PAD_STATE_COALESCE).
void CheckTapAcrossStickBurst() {
  static PadData data[64];
  (void)Read(data);
  SetButton(HOST_INPUT_CONTROLLER_ID, PAD_BUTTON_CROSS, true);
  for (int i = 0; i < 200; i++) {
    SetAxis(HOST_INPUT_CONTROLLER_ID, Axis::LeftX, 100 + i % 50);
  }
  SetButton(HOST_INPUT_CONTROLLER_ID, PAD_BUTTON_CROSS, false);
  for (int i = 0; i < 200; i++) {
    SetAxis(HOST_INPUT_CONTROLLER_ID, Axis::LeftX, 150 - i % 50);
  }
  const int count = Read(data);
  bool pressed = false;
  bool released_after = false;
  for (int i = 0; i < count; i++) {
    if ((data[i].buttons & PAD_BUTTON_CROSS) != 0) {
      pressed = true;
    } else if (pressed) {
      released_after = true;
    }
  }
  Check(pressed && released_after, "a tap between two reads keeps its press and its release");
  Check(count > 0 && data[count - 1].left_stick_x == 150 - 199 % 50,
        "the newest state has the sticks' last values");
}

void CheckTriggersAndReconnect() {
  static PadData data[64];
  constexpr int id = 12345;
  Connect(id);
  SetAxis(id, Axis::TriggerLeft, 200);
  SetAxis(id, Axis::TriggerRight, 255);
  SetButton(id, PAD_BUTTON_CROSS, true);
  SetAxis(id, Axis::RightX, 255);
  Check(PadReadState(1, &data[0]) == 0, "read active controller");
  Check(data[0].analog_buttons_l2 == 200 && data[0].analog_buttons_r2 == 255,
        "analog triggers reach PS5 pad");
  Check((data[0].buttons & (PAD_BUTTON_L2 | PAD_BUTTON_R2)) == (PAD_BUTTON_L2 | PAD_BUTTON_R2),
        "analog triggers set digital trigger buttons");
  SetAxis(id, Axis::TriggerLeft, 0);
  SetAxis(id, Axis::TriggerRight, 0);
  Check(PadReadState(1, &data[0]) == 0 && (data[0].buttons & (PAD_BUTTON_L2 | PAD_BUTTON_R2)) == 0,
        "released triggers clear digital buttons");
  Disconnect(id);
  Disconnect(id); // An unopened/already removed device is harmless.
  Connect(id + 1);
  SetButton(id, PAD_BUTTON_CROSS, true); // Stale instance ID must be ignored.
  Check(PadReadState(1, &data[0]) == 0 && data[0].buttons == 0 && data[0].right_stick_x == 128,
        "reconnect resets buttons and sticks and ignores stale events");
  Check(data[0].analog_buttons_l2 == 0 && data[0].analog_buttons_r2 == 0,
        "reconnect resets triggers");
  Disconnect(id + 1);
}

} // namespace

int main() {
  Common::InitializeThreads();
  Common::Subsystems subsystems;
  subsystems.Initialize<Config::Lifecycle>();
  Config::ConfigOptions options;
  options.printf_direction = Config::OutputDirection::Silent;
  Config::Load(options);
  subsystems.Initialize<Log::Lifecycle>();

  Libs::Controller::Initialize();
  CheckTapAcrossStickBurst();
  CheckTriggersAndReconnect();
  Libs::Controller::Shutdown();
  subsystems.Destroy();

  std::printf("PadInputTests: all cases passed\n");
  return 0;
}
