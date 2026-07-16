#ifndef INCLUDED_SwgCuiPerformanceSongPicker_H
#define INCLUDED_SwgCuiPerformanceSongPicker_H

#include "clientUserInterface/CuiMediator.h"
#include "UIEventCallback.h"

class UIButton;
class UIComboBox;
class UIText;

class SwgCuiPerformanceSongPicker : public CuiMediator, public UIEventCallback
{
public:
	explicit SwgCuiPerformanceSongPicker(UIPage &page);
	virtual void OnButtonPressed(UIWidget *context);

protected:
	virtual void performActivate();
	virtual void performDeactivate();

private:
	virtual ~SwgCuiPerformanceSongPicker();
	SwgCuiPerformanceSongPicker();
	SwgCuiPerformanceSongPicker(SwgCuiPerformanceSongPicker const &);
	SwgCuiPerformanceSongPicker &operator=(SwgCuiPerformanceSongPicker const &);

	void refreshSongs();

	UIComboBox *m_songCombo;
	UIText *m_statusText;
	UIButton *m_startButton;
	UIButton *m_cancelButton;
};

#endif
