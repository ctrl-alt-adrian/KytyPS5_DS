#ifndef CONTROLLER_DIAGNOSTICS_DIALOG_H_
#define CONTROLLER_DIAGNOSTICS_DIALOG_H_

#include <QDialog>

#include <memory>

class ControllerDiagnosticsDialog: public QDialog {
public:
	explicit ControllerDiagnosticsDialog(QWidget* parent = nullptr);
	~ControllerDiagnosticsDialog() override;

private:
	class State;
	std::unique_ptr<State> m_state;
};

#endif
