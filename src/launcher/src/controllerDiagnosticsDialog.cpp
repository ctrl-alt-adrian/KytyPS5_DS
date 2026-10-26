#include "controllerDiagnosticsDialog.h"

#include "SDL.h"
#include "common/sdlController.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <map>
#include <utility>

class ControllerDiagnosticsDialog::State {
public:
	struct Device {
		SDL_GameController* controller = nullptr;
		SDL_Joystick*       joystick = nullptr; // Borrowed from controller, or independently owned.
	};

	QPlainTextEdit*                  devices     = nullptr;
	QPlainTextEdit*                  input       = nullptr;
	QPlainTextEdit*                  history     = nullptr;
	QTimer*                          timer       = nullptr;
	bool                             initialized = false;
	std::map<SDL_JoystickID, Device> opened;

	~State() {
		timer->stop();
		CloseDevices();
		if (initialized) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	}

	void Log(const QString& message) {
		history->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss.zzz ") + message);
	}

	static void CloseDevice(const Device& device) {
		if (device.controller != nullptr)
			SDL_GameControllerClose(device.controller);
		else if (device.joystick != nullptr)
			SDL_JoystickClose(device.joystick);
	}

	void CloseDevices() {
		for (const auto& [id, device]: opened)
			CloseDevice(device);
		opened.clear();
	}

	void Refresh() {
		if (!initialized) return;
		auto previous = std::move(opened);
		opened.clear();
		SDL_ClearError();
		const int   count = SDL_NumJoysticks();
		QStringList lines;
		if (count < 0) {
			Log(QString("Enumeration failed: %1").arg(SDL_GetError()));
		} else if (count == 0) {
			lines << "No SDL joystick devices detected. Desktop keyboard/mouse emulation alone is "
			         "not a gamepad.";
		}
		for (int index = 0; index < count; index++) {
			const auto id = SDL_JoystickGetDeviceInstanceID(index);
			char       guid[33] {};
			SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof(guid));
			const char* name   = SDL_JoystickNameForIndex(index);
			const bool  mapped = SDL_IsGameController(index) == SDL_TRUE;
			lines << QString("Device %1 / instance %2: %3\nGUID: %4\nVID: %5  PID: %6 (0000 = "
			                 "unavailable)\nSDL_IsGameController: %7")
			             .arg(index)
			             .arg(id)
			             .arg(name != nullptr ? QString::fromUtf8(name) : "Unknown")
			             .arg(guid)
			             .arg(SDL_JoystickGetDeviceVendor(index), 4, 16, QLatin1Char('0'))
			             .arg(SDL_JoystickGetDeviceProduct(index), 4, 16, QLatin1Char('0'))
			             .arg(mapped ? "yes" : "no");
			Device device;
			if (auto it = previous.find(id); it != previous.end()) {
				device = it->second;
				previous.erase(it);
				// Keep attached devices open across refreshes; reopening can reset native hardware.
				if (mapped != (device.controller != nullptr) ||
				    (device.joystick != nullptr &&
				     SDL_JoystickGetAttached(device.joystick) == SDL_FALSE)) {
					CloseDevice(device);
					device = {};
				}
			}
			SDL_ClearError();
			if (mapped) {
				if (device.controller == nullptr) device.controller = SDL_GameControllerOpen(index);
				if (device.controller != nullptr) {
					device.joystick = SDL_GameControllerGetJoystick(device.controller);
					lines << "GameController open: success";
				} else {
					lines << QString("GameController open failed: %1").arg(SDL_GetError());
				}
				SDL_ClearError();
				char* mapping = SDL_GameControllerMappingForDeviceIndex(index);
				lines << (mapping != nullptr
				              ? QString("Mapping: %1").arg(QString::fromUtf8(mapping))
							  : QString("Mapping unavailable: %1").arg(SDL_GetError()));
				SDL_free(mapping);
			} else {
				lines << "Mapping: unavailable; raw joystick input only. Supply an SDL mapping via "
				         "SDL_GAMECONTROLLERCONFIG.";
			}
			if (device.joystick == nullptr) {
				SDL_ClearError();
				device.joystick = SDL_JoystickOpen(index);
			}
			if (device.controller == nullptr) {
				lines << (device.joystick != nullptr
				              ? "Joystick open: success"
							  : QString("Joystick open failed: %1").arg(SDL_GetError()));
			}
			if (id >= 0)
				opened.emplace(id, device);
			else {
				CloseDevice(device);
				lines << QString("Instance ID unavailable: %1").arg(SDL_GetError());
			}
			lines << "";
		}
		for (const auto& [id, device]: previous)
			CloseDevice(device);
		devices->setPlainText(lines.join('\n'));
		UpdateInput();
	}

	void UpdateInput() {
		QStringList lines;
		for (const auto& [id, device]: opened) {
			if (device.joystick == nullptr || SDL_JoystickGetAttached(device.joystick) == SDL_FALSE)
				continue;
			lines << QString("Instance %1").arg(id);
			QStringList buttons;
			QStringList axes;
			if (device.controller != nullptr) {
				for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) {
					const auto button = static_cast<SDL_GameControllerButton>(b);
					if (SDL_GameControllerGetButton(device.controller, button) != 0)
						buttons << QString::fromUtf8(SDL_GameControllerGetStringForButton(button));
				}
				for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++) {
					const auto axis = static_cast<SDL_GameControllerAxis>(a);
					axes << QString("%1=%2")
					            .arg(SDL_GameControllerGetStringForAxis(axis))
					            .arg(SDL_GameControllerGetAxis(device.controller, axis));
				}
			} else {
				for (int b = 0; b < SDL_JoystickNumButtons(device.joystick); b++) {
					if (SDL_JoystickGetButton(device.joystick, b) != 0)
						buttons << QString::number(b);
				}
				for (int a = 0; a < SDL_JoystickNumAxes(device.joystick); a++)
					axes
					    << QString("axis%1=%2").arg(a).arg(SDL_JoystickGetAxis(device.joystick, a));
				for (int h = 0; h < SDL_JoystickNumHats(device.joystick); h++)
					axes << QString("hat%1=%2").arg(h).arg(SDL_JoystickGetHat(device.joystick, h));
			}
			lines << "Pressed: " + (buttons.isEmpty() ? "none" : buttons.join(", "));
			lines << axes.join("  ");
		}
		input->setPlainText(lines.join('\n'));
	}

	void Tick() {
		// This process has one SDL consumer. Qt owns the GUI loop; no SDL video window is created.
		SDL_PumpEvents();
		SDL_Event events[64];
		bool      refresh = false;
		// Bound work per Qt tick and remove only joystick/controller events.
		for (int batch = 0; batch < 4; batch++) {
			const int count = SDL_PeepEvents(events, 64, SDL_GETEVENT, SDL_JOYAXISMOTION,
			                                 SDL_CONTROLLERSENSORUPDATE);
			if (count < 0) {
				Log(QString("Event read failed: %1").arg(SDL_GetError()));
				break;
			}
			for (int i = 0; i < count; i++) {
				const auto& event = events[i];
				if (event.type == SDL_JOYDEVICEADDED) {
					Log(QString("Connected: device index %1").arg(event.jdevice.which));
					refresh = true;
				} else if (event.type == SDL_JOYDEVICEREMOVED) {
					Log(QString("Disconnected: instance %1").arg(event.jdevice.which));
					refresh = true;
				} else if (event.type == SDL_CONTROLLERDEVICEADDED) {
					// A mapping can turn an already-present raw joystick into a controller.
					refresh = true;
				} else if (event.type == SDL_CONTROLLERDEVICEREMAPPED) {
					Log(QString("Mapping changed: instance %1").arg(event.cdevice.which));
					refresh = true;
				}
			}
			if (count < 64) break;
		}
		if (refresh)
			Refresh();
		else
			UpdateInput();
	}
};

ControllerDiagnosticsDialog::ControllerDiagnosticsDialog(QWidget* parent)
    : QDialog(parent), m_state(std::make_unique<State>()) {
	setWindowTitle(tr("Controller Diagnostics"));
	resize(850, 700);
	auto* layout = new QVBoxLayout(this);
	auto* help   = new QLabel(
	    tr("SDL buttons: A=Cross, B=Circle, X=Square, Y=Triangle; shoulders=L1/R1; "
		   "Start=Options; Back=touchpad on non-PlayStation pads. Sticks: -32768..32767; triggers: "
		   "0..32767. "
		   "Unavailable controls depend on the SDL mapping. Close this dialog before playing."),
	    this);
	help->setWordWrap(true);
	layout->addWidget(help);
	m_state->devices = new QPlainTextEdit(this);
	m_state->input   = new QPlainTextEdit(this);
	m_state->history = new QPlainTextEdit(this);
	m_state->history->setMaximumBlockCount(200);
	for (auto* text: {m_state->devices, m_state->input, m_state->history}) {
		text->setReadOnly(true);
		layout->addWidget(text);
	}
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	auto* refresh = buttons->addButton(tr("Refresh"), QDialogButtonBox::ActionRole);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(refresh, &QPushButton::clicked, this, [this] { m_state->Refresh(); });
	m_state->timer = new QTimer(this);
	connect(m_state->timer, &QTimer::timeout, this, [this] { m_state->Tick(); });

	SDL_SetMainReady();
	HostController::SetHints();
	// There is no SDL video window to own focus in this Qt process.
	SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1", SDL_HINT_DEFAULT);
	SDL_ClearError();
	if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
		m_state->Log(QString("SDL initialization failed: %1").arg(SDL_GetError()));
		refresh->setEnabled(false);
		return;
	}
	m_state->initialized = true;
	SDL_version version;
	SDL_GetVersion(&version);
	m_state->Log(QString("SDL %1.%2.%3").arg(version.major).arg(version.minor).arg(version.patch));
	for (const char* hint: {SDL_HINT_JOYSTICK_HIDAPI, SDL_HINT_JOYSTICK_HIDAPI_STEAM,
	                        "SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD",
	                        SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "SDL_GAMECONTROLLERCONFIG",
	                        "SDL_GAMECONTROLLERCONFIG_FILE"}) {
		const char* value = SDL_GetHint(hint);
		m_state->Log(QString("%1=%2").arg(hint).arg(value != nullptr ? QString::fromUtf8(value)
		                                                             : "<SDL default>"));
	}
	m_state->Refresh();
	m_state->timer->start(33);
}

ControllerDiagnosticsDialog::~ControllerDiagnosticsDialog() = default;
