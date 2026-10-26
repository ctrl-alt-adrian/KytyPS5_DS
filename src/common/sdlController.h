#ifndef KYTY_COMMON_SDL_CONTROLLER_H_
#define KYTY_COMMON_SDL_CONTROLLER_H_

#include "SDL_hints.h"

namespace HostController {

inline void SetHints() {
	// The pinned SDL2 Steam HIDAPI backend is opt-in. Keep user/environment overrides.
	SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1", SDL_HINT_DEFAULT);
	// SDL2 otherwise filters Steam's virtual gamepad before GameController enumeration.
	SDL_SetHintWithPriority("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", "1",
	                        SDL_HINT_DEFAULT);
}

} // namespace HostController

#endif
