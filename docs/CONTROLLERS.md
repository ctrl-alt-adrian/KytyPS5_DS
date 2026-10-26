# SDL2 controller diagnostics and Steam Controller validation

Open **Controller Diagnostics** in the Qt launcher's main window before starting a game.
No game or game configuration is required. The modal dialog services SDL on the Qt GUI
thread every 33 ms, without creating an SDL video window or a second event thread.
It consumes only joystick/controller events. The button is disabled while the launcher's
emulator process is running; close its console too if it remains open after the game exits.
Closing the dialog releases all its device handles and its SDL subsystem reference before
starting the emulator in its separate process.

The device pane shows names, instance IDs, GUIDs, vendor/product IDs (0000 means unavailable),
GameController recognition, mappings, and open results/errors. The live pane shows pressed
SDL buttons and all six mapped axes, or raw buttons/axes/hats for unmapped joysticks.
The history pane shows the SDL version, effective hints, initialization/enumeration errors,
and connection/disconnection/remapping events (the most recent 200 lines).
Use **Refresh** to retry device opens; attached open devices retain their handles.
Select and copy pane text when reporting a problem. Device indices can change after unplug;
instance IDs identify a connection and change on reconnect.

## Findings and compatibility change

The performance fork already translates SDL2 GameController events into the existing PS5
pad state history read by scePadRead/scePadReadState. Keyboard/mouse input shares that state.
Controller add events carry a device index for SDL_GameControllerOpen; input/removal events
carry an instance ID. Unknown removals are already ignored, and changing the active controller
resets input state. No rewrite of hostInput.cpp or controller.cpp is required.

The pinned SDL2 submodule (4b69833bc54abf3dd3288d4aa7afbba527775e5b, reporting 2.33.0)
has a Steam Controller HIDAPI backend, but HIDAPI_DriverSteam_IsEnabled defaults to false.
Its SDL_hints.h describes this as the Bluetooth Steam Controller backend. SDL2 also filters
Steam virtual gamepads unless SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD is enabled.
Both launcher diagnostics and the emulator now request these defaults before initializing
controllers:

- SDL_JOYSTICK_HIDAPI_STEAM=1
- SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD=1

These use SDL_HINT_DEFAULT, so environment variables and higher-priority hints take precedence.
The diagnostics additionally requests background controller input because its window belongs
to Qt. Existing PS4/PS5 rumble settings and the game event loop are preserved.
The original upstream's current main branch uses SDL3; that implementation was inspected as
context, not ported into this SDL2 fork. No Steamworks SDK or additional runtime dependency
was added. No GPU or shader code was changed.

This establishes disabled SDL paths, not the cause of a particular physical controller's failure.
USB, wireless receiver, and Bluetooth must be tested separately. Native HID access, device
permissions, firmware, and Steam's ownership of the device can affect results.
No arbitrary GUID mapping or Steam-specific button indices have been added.

## Input mapping

| SDL GameController input | PS5 pad input |
| --- | --- |
| A / B / X / Y | Cross / Circle / Square / Triangle |
| Left / right shoulder | L1 / R1 |
| Left / right trigger axis | Analog L2 / R2 and their digital buttons |
| Left / right stick click | L3 / R3 |
| D-pad directions | D-pad directions |
| Left / right X and Y axes | Left / right analog sticks |
| Start | Options |
| Touchpad button and SDL touchpad finger events | Touchpad click and contacts |
| Back / View / Select on non-PS4/PS5 controllers | Touchpad-equivalent click |

Sticks translate from -32768..32767 to 0..255; trigger axes translate from 0..32767
to 0..255. The existing digital trigger threshold is any translated value greater than zero.
Physical touch contacts, stick clicks, or a right-stick equivalent are available only if the
SDL backend/mapping exposes them. Steam Controller trackpads are not assumed to be PS5 touchpads.
Existing Guide/Back+Start hold-to-quit behavior remains in effect.

## Windows build

Use the repository's Windows prerequisites: Visual Studio 2022 x64 C++ tools, LLVM clang-cl,
Ninja, CMake, Vulkan SDK with glslangValidator, Python 3, and Qt6 MSVC x64 with Concurrent,
Network, and Widgets. In an x64 Developer PowerShell:

```powershell
git submodule update --init --recursive
cmake -S . -B _Build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DKYTY_BUILD_LAUNCHER=ON -DBUILD_TESTING=ON -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build _Build/windows --target launcher controller_mapping_tests controller_diagnostics_tests pad_input_tests
ctest --test-dir _Build/windows -R '^(controller_mapping|controller_diagnostics|pad_input)$' --output-on-failure
cmake --install _Build/windows --prefix _Build/windows/install
```

Replace the Qt path. Explicitly enable KYTY_BUILD_LAUNCHER if reusing a build directory
created by build-windows.cmd, which normally disables the Qt launcher. The diagnostics test
uses Qt's offscreen platform and SDL virtual devices, so it needs no physical controller.
For tests, add your Qt bin directory to PATH; windeployqt deploys Qt alongside the launcher,
while the standalone diagnostics test may need Qt from that PATH. Use the install directory
for the runnable launcher package; no new SDL2 DLL is needed because SDL2 is linked statically.

## Physical Windows test matrix

Record the connection method, firmware if known, SDL version, effective hints, device identity,
recognition, mapping, and open result for every scenario. For each recognized controller,
exercise every mapped control above in diagnostics, close the dialog, then verify those
controls in Demon's Souls. Also verify Xbox, DualShock, or DualSense input if available.

1. **Steam completely closed:** Exit Steam from its tray menu and verify it has stopped.
   Connect the controller, open diagnostics, and test native enumeration and input.
   Repeat for each available connection method. A successful open without changing live
   input is a distinct failure from missing enumeration or a missing mapping.
2. **Steam running with Desktop Layout enabled:** Start Steam and repeat diagnostics.
   Record whether a native or virtual gamepad appears. Desktop Layout may expose only
   keyboard/mouse actions; working desktop pointer movement alone does not establish
   GameController input. Check whether opening/closing diagnostics restores desktop behavior.
3. **Steam Input exposing a gamepad:** Where supported, configure the relevant non-Steam
   executable/shortcut with a gamepad layout and launch through Steam. Repeat diagnostics
   in that launch context, then game testing. A virtual device visible to the emulator may
   depend on the shortcut/process context and may not appear in a separately started launcher.
   If native and virtual devices both appear, record both. To isolate Steam Input, repeat
   with native HIDAPI disabled as shown below; the emulator retains its existing first
   connected physical-controller selection policy.
4. **Disconnect and reconnect:** While diagnostics is open, disconnect the controller and
   confirm a removal entry and disappearance of its live state. Reconnect and confirm a new
   instance ID and working input without restarting the launcher. Repeat while holding a
   button or trigger before unplugging; there should be no stuck input. Test reconnect during
   gameplay too, then close/reopen diagnostics after exiting the game.

To compare behavior with native Steam HIDAPI disabled, start the launcher from PowerShell:

```powershell
$env:SDL_JOYSTICK_HIDAPI_STEAM = '0'
& .\_Build\windows\install\launcher.exe
Remove-Item Env:SDL_JOYSTICK_HIDAPI_STEAM
```

If a joystick opens but SDL_IsGameController is false, retain its GUID and raw input results.
Supply a verified mapping for that device/platform using SDL_GAMECONTROLLERCONFIG or
SDL_GAMECONTROLLERCONFIG_FILE before starting the launcher; SDL applies it in both processes.
Do not copy an unrelated controller's indices. If an open fails, retain the exact SDL error.
Changing environment hints requires restarting the launcher; Refresh retries device enumeration
and opening, not process environment configuration.

## Validation status in this checkout

Verified in WSL2 with the pinned SDL2 and Qt 6.4.2:

- SDL2 static library built successfully.
- ControllerMappingTests compiled with GCC 13, -Wall -Wextra -Werror, and passed: runtime
  button/axis translation, hint override priority, virtual controller mapping/input events,
  removal/reconnect IDs, and balanced SDL cleanup.
- The actual ControllerDiagnosticsDialog and ControllerDiagnosticsTests compiled with the
  same warnings enabled and passed using Qt offscreen: mapped and unmapped devices,
  live button/axis/hat display, Refresh, hotplug history, cleanup ownership, and reopening.
- The modified Qt Designer form was processed successfully by Qt uic; mainDialog.cpp and
  its generated UI/moc passed a C++ syntax check.

A full project configure was attempted but is blocked by other uninitialized third-party
submodules (including SPIRV-Tools and xxHash) and missing glslangValidator. The added
PadInputTests trigger/reconnect cases have therefore not been run. The focused checks used
SDL built separately and temporary Qt packages without a system installation.

No Windows build was performed in this WSL2 environment. No physical Steam Controller,
Steam Input session, or Demon's Souls gameplay was tested. Native/Steam Input hardware support,
connection-mode coverage, and regression checks with Xbox/DualShock/DualSense remain pending
the Windows matrix above. Passing virtual-device tests is not a claim of verified hardware support.

## Changed files

| Files | Purpose |
| --- | --- |
| src/launcher/include/controllerDiagnosticsDialog.h, src/launcher/src/controllerDiagnosticsDialog.cpp | Diagnostics UI, SDL lifetime, input polling, device opens, and history |
| src/launcher/forms/main_dialog.ui, src/launcher/src/mainDialog.cpp | Main-window button and process-state gating |
| src/launcher/CMakeLists.txt | Existing SDL2 static linkage and Qt diagnostics test target |
| src/common/sdlController.h | Shared Steam HIDAPI and virtual-gamepad defaults |
| src/graphics/presentation/window/window.cpp, controllerMapping.h in the same directory | Apply shared defaults and expose existing translations for tests |
| tests/ControllerMappingTests.cpp, tests/ControllerDiagnosticsTests.cpp | SDL virtual-device and real-dialog tests |
| tests/PadInputTests.cpp | PS5 pad trigger and reconnect regression cases |
| CMakeLists.txt | Mapping test registration and aggregate test dependency |
| docs/CONTROLLERS.md | Investigation, validation limits, and Windows build/test instructions |
