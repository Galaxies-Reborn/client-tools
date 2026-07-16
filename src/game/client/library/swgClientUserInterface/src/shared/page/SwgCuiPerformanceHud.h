#ifndef INCLUDED_SwgCuiPerformanceHud_H
#define INCLUDED_SwgCuiPerformanceHud_H

#include "clientUserInterface/CuiMediator.h"
#include "UIEventCallback.h"

class UIButton;
class UIText;

class SwgCuiPerformanceHud : public CuiMediator, public UIEventCallback
{
public:
	explicit SwgCuiPerformanceHud(UIPage &page);
	virtual void OnButtonPressed(UIWidget *context);
	virtual void update(float deltaTimeSecs);

protected:
	virtual void performActivate();
	virtual void performDeactivate();

private:
	virtual ~SwgCuiPerformanceHud();
	SwgCuiPerformanceHud();
	SwgCuiPerformanceHud(SwgCuiPerformanceHud const &);
	SwgCuiPerformanceHud &operator=(SwgCuiPerformanceHud const &);

	void updateDisplay();

	UIText *m_modeText;
	UIText *m_songText;
	UIText *m_deviceText;
	UIText *m_octaveText;
	UIButton *m_flourishButtons[8];
	UIButton *m_pauseButton;
	UIButton *m_settingsButton;
	UIButton *m_stopButton;
	float m_updateTimer;
	float m_failureTimer;
};

#endif
