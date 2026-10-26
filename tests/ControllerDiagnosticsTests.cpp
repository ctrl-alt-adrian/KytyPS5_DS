#include "SDL.h"
#include "controllerDiagnosticsDialog.h"

#include <QApplication>
#include <QEventLoop>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool value, const char* message) {
	if (!value) {
		std::fprintf(stderr, "ControllerDiagnosticsTests: %s (%s)\n", message, SDL_GetError());
		std::abort();
	}
}

QString Contents(const ControllerDiagnosticsDialog& dialog) {
	QString result;
	for (auto* text: dialog.findChildren<QPlainTextEdit*>())
		result += text->toPlainText() + '\n';
	return result;
}

void Tick() {
	QEventLoop loop;
	QTimer::singleShot(150, &loop, &QEventLoop::quit);
	loop.exec();
}

} // namespace

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);
	SDL_SetMainReady();
	Check(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0, "test SDL reference");
	{
		ControllerDiagnosticsDialog dialog;
		dialog.show();
		Tick();
		SDL_Event unrelated {};
		unrelated.type = SDL_USEREVENT;
		Check(SDL_PushEvent(&unrelated) == 1, "queue unrelated SDL event");
		Tick();
		Check(SDL_PeepEvents(&unrelated, 1, SDL_GETEVENT, SDL_USEREVENT, SDL_USEREVENT) == 1,
		      "diagnostics preserves unrelated SDL events");
		Check(Contents(dialog).contains("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD=1"),
		      "effective hints displayed");
		const int index =
		    SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX,
			                          SDL_CONTROLLER_BUTTON_MAX, 0);
		Check(index >= 0, "attach mapped controller with dialog open");
		Tick();
		Check(Contents(dialog).contains("GameController open: success"),
		      "dialog opens mapped device");
		Check(Contents(dialog).contains("GUID:") && Contents(dialog).contains("VID:") &&
		          Contents(dialog).contains("Mapping:"),
		      "identity and mapping displayed");
		auto* joystick = SDL_JoystickOpen(index);
		Check(joystick != nullptr, "test joystick handle");
		Check(SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, 1) == 0, "inject A");
		Tick();
		Check(Contents(dialog).contains("Pressed: a"), "live mapped face button");
		Check(Contents(dialog).contains("leftx=") && Contents(dialog).contains("righttrigger="),
		      "live mapped axes");
		for (auto* button: dialog.findChildren<QPushButton*>())
			if (button->text() == "Refresh") button->click();
		Check(Contents(dialog).contains("GameController open: success"), "manual refresh");
		Check(SDL_JoystickDetachVirtual(index) == 0, "unplug mapped device");
		SDL_JoystickClose(joystick);
		Tick();
		Check(Contents(dialog).contains("Disconnected: instance"), "disconnection history");
		Check(!Contents(dialog).contains("GameController open: success"),
		      "detached device removed from list");
		const int raw = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 4, 1);
		Check(raw >= 0, "attach raw joystick");
		Tick();
		Check(Contents(dialog).contains("SDL_IsGameController: no") &&
		          Contents(dialog).contains("Joystick open: success"),
		      "unmapped joystick remains diagnosable");
		Check(Contents(dialog).contains("axis0=") && Contents(dialog).contains("hat0="),
		      "raw live axes and hat");
		Check(SDL_JoystickDetachVirtual(raw) == 0, "unplug raw joystick");
		Tick();
	}
	Check(SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0,
	      "dialog preserves another subsystem reference");
	SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	Check(SDL_WasInit(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) == 0,
	      "dialog releases its subsystem reference");
	{
		ControllerDiagnosticsDialog reopened;
		Tick();
		Check(Contents(reopened).contains("SDL 2."), "dialog can reopen after cleanup");
	}
	Check(SDL_WasInit(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) == 0, "reopened dialog cleanup");
	std::puts("ControllerDiagnosticsTests: all cases passed");
}
