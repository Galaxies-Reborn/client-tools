#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiOptPerformance.h"

#include "UIButton.h"
#include "UIComboBox.h"
#include "UIPage.h"
#include "UIText.h"
#include "UnicodeUtils.h"
#include "clientGame/PerformanceModeManager.h"

#include <dinput.h>
#include <cstdio>
#include <vector>

namespace SwgCuiOptPerformanceNamespace
{
	struct KeyChoice
	{
		char const *name;
		int scanCode;
	};

	KeyChoice const s_keyChoices[] =
	{
		{"Unbound", 0},
		{"A", DIK_A}, {"B", DIK_B}, {"C", DIK_C}, {"D", DIK_D}, {"E", DIK_E}, {"F", DIK_F},
		{"G", DIK_G}, {"H", DIK_H}, {"I", DIK_I}, {"J", DIK_J}, {"K", DIK_K}, {"L", DIK_L},
		{"M", DIK_M}, {"N", DIK_N}, {"O", DIK_O}, {"P", DIK_P}, {"Q", DIK_Q}, {"R", DIK_R},
		{"S", DIK_S}, {"T", DIK_T}, {"U", DIK_U}, {"V", DIK_V}, {"W", DIK_W}, {"X", DIK_X},
		{"Y", DIK_Y}, {"Z", DIK_Z},
		{"1", DIK_1}, {"2", DIK_2}, {"3", DIK_3}, {"4", DIK_4}, {"5", DIK_5},
		{"6", DIK_6}, {"7", DIK_7}, {"8", DIK_8}, {"9", DIK_9}, {"0", DIK_0},
		{"Space", DIK_SPACE}, {"Minus", DIK_MINUS}, {"Equals", DIK_EQUALS},
		{"Left bracket", DIK_LBRACKET}, {"Right bracket", DIK_RBRACKET},
		{"Semicolon", DIK_SEMICOLON}, {"Apostrophe", DIK_APOSTROPHE},
		{"Comma", DIK_COMMA}, {"Period", DIK_PERIOD}, {"Slash", DIK_SLASH}
	};

	int const s_keyChoiceCount = sizeof(s_keyChoices) / sizeof(s_keyChoices[0]);

	int findKeyChoice(int scanCode)
	{
		for (int i = 0; i < s_keyChoiceCount; ++i)
			if (s_keyChoices[i].scanCode == scanCode)
				return i;
		return 0;
	}

	std::string midiNoteName(int note)
	{
		char const * const names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%s%d (%d)", names[note % 12], note / 12 - 1, note);
		return buffer;
	}
}

using namespace SwgCuiOptPerformanceNamespace;

SwgCuiOptPerformance::SwgCuiOptPerformance(UIPage &page) :
	SwgCuiOptBase("SwgCuiOptPerformance", page),
	m_midiDeviceCombo(0),
	m_octaveCombo(0),
	m_mappingModeCombo(0),
	m_flourishRows(0),
	m_musicRows(0),
	m_refreshButton(0),
	m_statusText(0),
	m_revertOctave(0),
	m_updatingControls(false)
{
	for (int i = 0; i < 8; ++i)
	{
		m_keyCombos[i] = 0;
		m_noteCombos[i] = 0;
		m_revertKeys[i] = 0;
		m_revertNotes[i] = 60 + i;

		char codeName[32];
		snprintf(codeName, sizeof(codeName), "keyFlourish%d", i + 1);
		getCodeDataObject(TUIComboBox, m_keyCombos[i], codeName);
		for (int choice = 0; choice < s_keyChoiceCount; ++choice)
			m_keyCombos[i]->AddItem(Unicode::narrowToWide(s_keyChoices[choice].name), s_keyChoices[choice].name);
		registerMediatorObject(*m_keyCombos[i], true);

		snprintf(codeName, sizeof(codeName), "noteFlourish%d", i + 1);
		getCodeDataObject(TUIComboBox, m_noteCombos[i], codeName);
		for (int note = 0; note < 128; ++note)
		{
			std::string const label = midiNoteName(note);
			m_noteCombos[i]->AddItem(Unicode::narrowToWide(label), label);
		}
		registerMediatorObject(*m_noteCombos[i], true);
	}
	for (int i = 0; i < 24; ++i)
	{
		m_musicKeyCombos[i] = 0;
		m_revertMusicKeys[i] = PerformanceModeManager::getDefaultMusicKey(i);
		char codeName[32];
		snprintf(codeName, sizeof(codeName), "musicKey%d", i + 1);
		getCodeDataObject(TUIComboBox, m_musicKeyCombos[i], codeName);
		for (int choice = 0; choice < s_keyChoiceCount; ++choice)
			m_musicKeyCombos[i]->AddItem(Unicode::narrowToWide(s_keyChoices[choice].name), s_keyChoices[choice].name);
		registerMediatorObject(*m_musicKeyCombos[i], true);
	}

	getCodeDataObject(TUIComboBox, m_midiDeviceCombo, "midiDevice");
	getCodeDataObject(TUIComboBox, m_octaveCombo, "midiOctave");
	getCodeDataObject(TUIComboBox, m_mappingModeCombo, "mappingMode");
	getCodeDataObject(TUIPage, m_flourishRows, "flourishRows");
	getCodeDataObject(TUIPage, m_musicRows, "musicRows");
	getCodeDataObject(TUIButton, m_refreshButton, "refreshMidi");
	getCodeDataObject(TUIText, m_statusText, "midiStatus");

	for (int octave = -4; octave <= 4; ++octave)
	{
		char label[24];
		snprintf(label, sizeof(label), "%+d octave%s", octave, octave == 1 || octave == -1 ? "" : "s");
		m_octaveCombo->AddItem(Unicode::narrowToWide(label), label);
	}
	m_mappingModeCombo->AddItem(Unicode::narrowToWide("Flourish mappings"), "flourish");
	m_mappingModeCombo->AddItem(Unicode::narrowToWide("Instrument keyboard"), "music");
	m_mappingModeCombo->SetSelectedIndex(0);

	registerMediatorObject(*m_midiDeviceCombo, true);
	registerMediatorObject(*m_octaveCombo, true);
	registerMediatorObject(*m_mappingModeCombo, true);
	registerMediatorObject(*m_refreshButton, true);
	m_statusText->SetPreLocalized(true);
	refreshMidiDevices();
	updateControls();
}

SwgCuiOptPerformance::~SwgCuiOptPerformance()
{
}

void SwgCuiOptPerformance::performActivate()
{
	SwgCuiOptBase::performActivate();
	refreshMidiDevices();
	updateControls();
}

void SwgCuiOptPerformance::OnButtonPressed(UIWidget *context)
{
	if (context == m_refreshButton)
	{
		refreshMidiDevices();
		updateControls();
		return;
	}
	SwgCuiOptBase::OnButtonPressed(context);
}

void SwgCuiOptPerformance::OnGenericSelectionChanged(UIWidget *context)
{
	if (m_updatingControls)
		return;

	for (int i = 0; i < 8; ++i)
	{
		if (context == m_keyCombos[i])
		{
			int const selection = m_keyCombos[i]->GetSelectedIndex();
			if (selection >= 0 && selection < s_keyChoiceCount)
				PerformanceModeManager::setFlourishKey(i, s_keyChoices[selection].scanCode);
			updateStatus();
			return;
		}
		if (context == m_noteCombos[i])
		{
			PerformanceModeManager::setFlourishMidiNote(i, m_noteCombos[i]->GetSelectedIndex());
			updateStatus();
			return;
		}
	}
	for (int i = 0; i < 24; ++i)
		if (context == m_musicKeyCombos[i])
		{
			int const selection = m_musicKeyCombos[i]->GetSelectedIndex();
			if (selection >= 0 && selection < s_keyChoiceCount)
				PerformanceModeManager::setMusicKey(i, s_keyChoices[selection].scanCode);
			updateStatus();
			return;
		}

	if (context == m_mappingModeCombo)
	{
		bool const music = m_mappingModeCombo->GetSelectedIndex() == 1;
		m_flourishRows->SetVisible(!music);
		m_musicRows->SetVisible(music);
		updateStatus();
	}
	else if (context == m_octaveCombo)
	{
		PerformanceModeManager::setMidiOctaveShift(m_octaveCombo->GetSelectedIndex() - 4);
		updateStatus();
	}
	else if (context == m_midiDeviceCombo)
	{
		std::string identifier;
		m_midiDeviceCombo->GetSelectedIndexName(identifier);
		std::string status;
		PerformanceModeManager::selectMidiDevice(identifier, status);
		m_statusText->SetLocalText(Unicode::narrowToWide(status));
	}
}

void SwgCuiOptPerformance::storeRevertData()
{
	SwgCuiOptBase::storeRevertData();
	for (int i = 0; i < 8; ++i)
	{
		m_revertKeys[i] = PerformanceModeManager::getFlourishKey(i);
		m_revertNotes[i] = PerformanceModeManager::getFlourishMidiNote(i);
	}
	for (int i = 0; i < 24; ++i)
		m_revertMusicKeys[i] = PerformanceModeManager::getMusicKey(i);
	m_revertOctave = PerformanceModeManager::getMidiOctaveShift();
	m_revertDeviceIdentifier = PerformanceModeManager::getSelectedMidiDeviceIdentifier();
}

void SwgCuiOptPerformance::revert()
{
	for (int i = 0; i < 8; ++i)
	{
		PerformanceModeManager::setFlourishKey(i, m_revertKeys[i]);
		PerformanceModeManager::setFlourishMidiNote(i, m_revertNotes[i]);
	}
	for (int i = 0; i < 24; ++i)
		PerformanceModeManager::setMusicKey(i, m_revertMusicKeys[i]);
	PerformanceModeManager::setMidiOctaveShift(m_revertOctave);
	std::string status;
	PerformanceModeManager::selectMidiDevice(m_revertDeviceIdentifier, status);
	updateControls();
	SwgCuiOptBase::revert();
}

void SwgCuiOptPerformance::resetDefaults(bool confirmed)
{
	if (!confirmed)
	{
		SwgCuiOptBase::resetDefaults(false);
		return;
	}
	PerformanceModeManager::resetInputMappings();
	updateControls();
}

void SwgCuiOptPerformance::refreshMidiDevices()
{
	std::string const selectedIdentifier = PerformanceModeManager::getSelectedMidiDeviceIdentifier();
	m_updatingControls = true;
	m_midiDeviceCombo->Clear();
	m_midiDeviceCombo->AddItem(Unicode::narrowToWide("Select a MIDI input"), "");
	std::vector<PerformanceModeManager::MidiDevice> const devices = PerformanceModeManager::getMidiDevices();
	int selectedIndex = 0;
	for (std::vector<PerformanceModeManager::MidiDevice>::const_iterator i = devices.begin(); i != devices.end(); ++i)
	{
		m_midiDeviceCombo->AddItem(Unicode::narrowToWide(i->name), i->identifier);
		if (i->identifier == selectedIdentifier)
			selectedIndex = static_cast<int>(i - devices.begin()) + 1;
	}
	m_midiDeviceCombo->SetSelectedIndex(selectedIndex);
	m_midiDeviceCombo->SetEnabled(true);
	m_updatingControls = false;
	updateStatus();
}

void SwgCuiOptPerformance::updateControls()
{
	m_updatingControls = true;
	for (int i = 0; i < 8; ++i)
	{
		m_keyCombos[i]->SetSelectedIndex(findKeyChoice(PerformanceModeManager::getFlourishKey(i)));
		m_noteCombos[i]->SetSelectedIndex(PerformanceModeManager::getFlourishMidiNote(i));
	}
	for (int i = 0; i < 24; ++i)
		m_musicKeyCombos[i]->SetSelectedIndex(findKeyChoice(PerformanceModeManager::getMusicKey(i)));
	m_octaveCombo->SetSelectedIndex(PerformanceModeManager::getMidiOctaveShift() + 4);
	bool const music = m_mappingModeCombo->GetSelectedIndex() == 1;
	m_flourishRows->SetVisible(!music);
	m_musicRows->SetVisible(music);
	m_updatingControls = false;
	updateStatus();
}

void SwgCuiOptPerformance::updateStatus()
{
	for (int i = 0; i < 8; ++i)
		for (int j = i + 1; j < 8; ++j)
			if (PerformanceModeManager::getFlourishKey(i) != 0 && PerformanceModeManager::getFlourishKey(i) == PerformanceModeManager::getFlourishKey(j))
			{
				char warning[96];
				snprintf(warning, sizeof(warning), "Keyboard conflict: Flourish %d and %d use the same key", i + 1, j + 1);
				m_statusText->SetLocalText(Unicode::narrowToWide(warning));
				return;
			}
	for (int i = 0; i < 24; ++i)
		for (int j = i + 1; j < 24; ++j)
			if (PerformanceModeManager::getMusicKey(i) != 0 && PerformanceModeManager::getMusicKey(i) == PerformanceModeManager::getMusicKey(j))
			{
				char warning[112];
				snprintf(warning, sizeof(warning), "Instrument keyboard conflict: notes %d and %d use the same key", i + 1, j + 1);
				m_statusText->SetLocalText(Unicode::narrowToWide(warning));
				return;
			}

	std::string const &deviceName = PerformanceModeManager::getMidiDeviceName();
	m_statusText->SetLocalText(Unicode::narrowToWide(deviceName.empty() ? "No MIDI input selected; performance keyboard remains available" : "MIDI input: " + deviceName));
}
