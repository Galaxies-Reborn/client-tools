#ifndef INCLUDED_SwgCuiOptPerformance_H
#define INCLUDED_SwgCuiOptPerformance_H

#include "swgClientUserInterface/SwgCuiOptBase.h"

class UIButton;
class UIComboBox;
class UIText;

class SwgCuiOptPerformance : public SwgCuiOptBase
{
public:
	explicit SwgCuiOptPerformance(UIPage &page);
	virtual void OnButtonPressed(UIWidget *context);
	virtual void OnGenericSelectionChanged(UIWidget *context);
	virtual void storeRevertData();
	virtual void revert();
	virtual void resetDefaults(bool confirmed);

protected:
	virtual void performActivate();

private:
	virtual ~SwgCuiOptPerformance();
	SwgCuiOptPerformance();
	SwgCuiOptPerformance(SwgCuiOptPerformance const &);
	SwgCuiOptPerformance &operator=(SwgCuiOptPerformance const &);

	void refreshMidiDevices();
	void updateControls();
	void updateStatus();

	UIComboBox *m_keyCombos[8];
	UIComboBox *m_noteCombos[8];
	UIComboBox *m_midiDeviceCombo;
	UIComboBox *m_octaveCombo;
	UIButton *m_refreshButton;
	UIText *m_statusText;
	int m_revertKeys[8];
	int m_revertNotes[8];
	int m_revertOctave;
	std::string m_revertDeviceIdentifier;
	bool m_updatingControls;
};

#endif
