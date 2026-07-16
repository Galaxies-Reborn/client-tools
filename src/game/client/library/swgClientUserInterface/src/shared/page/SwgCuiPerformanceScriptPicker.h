#ifndef INCLUDED_SwgCuiPerformanceScriptPicker_H
#define INCLUDED_SwgCuiPerformanceScriptPicker_H

#include "clientUserInterface/CuiMediator.h"
#include "UIEventCallback.h"

class UIButton;
class UIComboBox;
class UIText;

class SwgCuiPerformanceScriptPicker : public CuiMediator, public UIEventCallback
{
public:
	explicit SwgCuiPerformanceScriptPicker(UIPage &page);
	virtual void OnButtonPressed(UIWidget *context);

protected:
	virtual void performActivate();
	virtual void performDeactivate();

private:
	virtual ~SwgCuiPerformanceScriptPicker();
	SwgCuiPerformanceScriptPicker();
	SwgCuiPerformanceScriptPicker(SwgCuiPerformanceScriptPicker const &);
	SwgCuiPerformanceScriptPicker &operator=(SwgCuiPerformanceScriptPicker const &);

	void refreshScripts();

	UIComboBox *m_scriptCombo;
	UIText *m_statusText;
	UIButton *m_refreshButton;
	UIButton *m_startButton;
	UIButton *m_cancelButton;
};

#endif
