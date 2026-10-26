#ifndef KYTY_GRAPHICS_PRESENTATION_WINDOW_CONTROLLER_MAPPING_H_
#define KYTY_GRAPHICS_PRESENTATION_WINDOW_CONTROLLER_MAPPING_H_

#include "SDL_gamecontroller.h"
#include "libs/controller.h"

namespace Libs::Graphics {

inline uint32_t
ControllerButtonToPadButton(int button, SDL_GameControllerType type = SDL_CONTROLLER_TYPE_UNKNOWN) {
	switch (button) {
		case SDL_CONTROLLER_BUTTON_A: return Controller::PAD_BUTTON_CROSS;
		case SDL_CONTROLLER_BUTTON_B: return Controller::PAD_BUTTON_CIRCLE;
		case SDL_CONTROLLER_BUTTON_X: return Controller::PAD_BUTTON_SQUARE;
		case SDL_CONTROLLER_BUTTON_Y: return Controller::PAD_BUTTON_TRIANGLE;
		case SDL_CONTROLLER_BUTTON_START: return Controller::PAD_BUTTON_OPTIONS;
		case SDL_CONTROLLER_BUTTON_LEFTSTICK: return Controller::PAD_BUTTON_L3;
		case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return Controller::PAD_BUTTON_R3;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return Controller::PAD_BUTTON_L1;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Controller::PAD_BUTTON_R1;
		case SDL_CONTROLLER_BUTTON_DPAD_UP: return Controller::PAD_BUTTON_UP;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return Controller::PAD_BUTTON_DOWN;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return Controller::PAD_BUTTON_LEFT;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Controller::PAD_BUTTON_RIGHT;
		// View/Back/Select is the touchpad equivalent on pads without a PlayStation touchpad.
		case SDL_CONTROLLER_BUTTON_BACK:
			return type != SDL_CONTROLLER_TYPE_PS4 && type != SDL_CONTROLLER_TYPE_PS5
			           ? Controller::PAD_BUTTON_TOUCH_PAD
					   : 0;
		case SDL_CONTROLLER_BUTTON_TOUCHPAD: return Controller::PAD_BUTTON_TOUCH_PAD;
		default: return 0;
	}
}

inline Controller::Axis ControllerAxisFromSdl(int axis_id) {
	switch (axis_id) {
		case SDL_CONTROLLER_AXIS_LEFTX: return Controller::Axis::LeftX;
		case SDL_CONTROLLER_AXIS_LEFTY: return Controller::Axis::LeftY;
		case SDL_CONTROLLER_AXIS_RIGHTX: return Controller::Axis::RightX;
		case SDL_CONTROLLER_AXIS_RIGHTY: return Controller::Axis::RightY;
		case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return Controller::Axis::TriggerLeft;
		case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return Controller::Axis::TriggerRight;
		default: return Controller::Axis::AxisMax;
	}
}

inline bool ControllerAxisIsTrigger(int axis_id) {
	return axis_id == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
	       axis_id == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
}

inline int ControllerAxisValueFromSdl(int axis_id, int axis_value) {
	return ControllerAxisIsTrigger(axis_id)
	           ? Controller::controller_get_axis(0, SDL_JOYSTICK_AXIS_MAX, axis_value)
			   : Controller::controller_get_axis(SDL_JOYSTICK_AXIS_MIN, SDL_JOYSTICK_AXIS_MAX,
			                                     axis_value);
}

} // namespace Libs::Graphics

#endif
