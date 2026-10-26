// Exercise the actual runtime translation and SDL2 mapping/hotplug paths without hardware.
#include "SDL.h"
#include "common/sdlController.h"
#include "graphics/presentation/window/controllerMapping.h"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool value, const char* message) {
	if (!value) {
		std::fprintf(stderr, "ControllerMappingTests: %s (%s)\n", message, SDL_GetError());
		std::abort();
	}
}

void CheckTranslation() {
	using namespace Libs::Graphics;
	using namespace Libs::Controller;
	constexpr std::array expected = {PAD_BUTTON_CROSS,    PAD_BUTTON_CIRCLE,    PAD_BUTTON_SQUARE,
	                                 PAD_BUTTON_TRIANGLE, PAD_BUTTON_TOUCH_PAD, uint32_t {0},
	                                 PAD_BUTTON_OPTIONS,  PAD_BUTTON_L3,        PAD_BUTTON_R3,
	                                 PAD_BUTTON_L1,       PAD_BUTTON_R1,        PAD_BUTTON_UP,
	                                 PAD_BUTTON_DOWN,     PAD_BUTTON_LEFT,      PAD_BUTTON_RIGHT};
	for (size_t i = 0; i < expected.size(); i++)
		Check(ControllerButtonToPadButton(static_cast<int>(i)) == expected[i],
		      "SDL button to PS5 button");
	Check(ControllerButtonToPadButton(SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_TYPE_PS4) == 0,
	      "PS4 Back is separate");
	Check(ControllerButtonToPadButton(SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_TYPE_PS5) == 0,
	      "PS5 Back is separate");
	Check(ControllerButtonToPadButton(SDL_CONTROLLER_BUTTON_TOUCHPAD) == PAD_BUTTON_TOUCH_PAD,
	      "native touchpad click");
	Check(ControllerButtonToPadButton(-1) == 0, "unknown button ignored");
	constexpr std::array axes = {Axis::LeftX,  Axis::LeftY,       Axis::RightX,
	                             Axis::RightY, Axis::TriggerLeft, Axis::TriggerRight};
	for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++) {
		Check(ControllerAxisFromSdl(a) == axes[a], "axis identity");
		Check(ControllerAxisValueFromSdl(a, 32767) == 255, "axis maximum");
		Check(ControllerAxisValueFromSdl(a, a >= SDL_CONTROLLER_AXIS_TRIGGERLEFT ? 0 : -32768) == 0,
		      "axis minimum");
		if (a < SDL_CONTROLLER_AXIS_TRIGGERLEFT)
			Check(ControllerAxisValueFromSdl(a, 0) >= 127 &&
			          ControllerAxisValueFromSdl(a, 0) <= 128,
			      "stick centre");
	}
	Check(ControllerAxisFromSdl(-1) == Axis::AxisMax, "unknown axis ignored");
}

void CheckVirtualController() {
	SDL_VirtualJoystickDesc desc {};
	desc.version     = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
	desc.type        = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
	desc.naxes       = SDL_CONTROLLER_AXIS_MAX;
	desc.nbuttons    = SDL_CONTROLLER_BUTTON_MAX;
	desc.button_mask = (1u << SDL_CONTROLLER_BUTTON_MAX) - 1u;
	desc.axis_mask   = (1u << SDL_CONTROLLER_AXIS_MAX) - 1u;
	desc.name        = "Kyty controller test";
	const int index  = SDL_JoystickAttachVirtualEx(&desc);
	Check(index >= 0, "attach virtual controller");
	Check(SDL_IsGameController(index) == SDL_TRUE, "virtual controller is mapped");
	auto* controller = SDL_GameControllerOpen(index);
	Check(controller != nullptr, "open mapped controller");
	auto*      joystick = SDL_GameControllerGetJoystick(controller);
	const auto id       = SDL_JoystickInstanceID(joystick);
	char*      mapping  = SDL_GameControllerMapping(controller);
	Check(mapping != nullptr, "retrieve SDL mapping");
	SDL_free(mapping);
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
	Check(SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, SDL_PRESSED) == 0,
	      "press virtual A");
	Check(SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, 20000) == 0,
	      "move virtual right stick");
	SDL_PumpEvents();
	Check(SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0, "mapped A state");
	Check(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX) == 20000,
	      "mapped right stick state");
	bool      press = false;
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.which == id &&
		    event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
			press = true;
	}
	Check(press, "mapped button event includes instance ID");
	Check(SDL_JoystickDetachVirtual(index) == 0, "detach controller");
	SDL_PumpEvents();
	Check(SDL_GameControllerGetAttached(controller) == SDL_FALSE, "open handle detects removal");
	bool removed = false;
	while (SDL_PollEvent(&event))
		if (event.type == SDL_CONTROLLERDEVICEREMOVED && event.cdevice.which == id) removed = true;
	Check(removed, "controller removal event");
	SDL_GameControllerClose(controller);
	const int second = SDL_JoystickAttachVirtualEx(&desc);
	Check(second >= 0, "reconnect controller");
	Check(SDL_JoystickGetDeviceInstanceID(second) != id, "reconnect uses a new instance ID");
	controller = SDL_GameControllerOpen(second);
	Check(controller != nullptr &&
	          SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) == 0,
	      "reconnect has no stuck A");
	SDL_GameControllerClose(controller);
	Check(SDL_JoystickDetachVirtual(second) == 0, "detach reconnected controller");
}

} // namespace

int main() {
	SDL_SetMainReady();
	HostController::SetHints();
	Check(SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_STEAM, SDL_FALSE) == SDL_TRUE,
	      "Steam HIDAPI enabled");
	Check(SDL_GetHintBoolean("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", SDL_FALSE) ==
	          SDL_TRUE,
	      "Steam virtual gamepads allowed");
	SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "0", SDL_HINT_OVERRIDE);
	HostController::SetHints();
	Check(SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_STEAM, SDL_TRUE) == SDL_FALSE,
	      "user override preserved");
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	Check(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0, "initialize SDL without video");
	CheckTranslation();
	CheckVirtualController();
	SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	Check(SDL_WasInit(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) == 0,
	      "balanced subsystem cleanup");
	std::puts("ControllerMappingTests: all cases passed");
}
