#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiPerformanceScriptPicker.h"

#include "UIButton.h"
#include "UIComboBox.h"
#include "UIPage.h"
#include "UIText.h"
#include "UnicodeUtils.h"
#include "clientGame/PerformanceModeManager.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "swgClientUserInterface/SwgCuiMediatorTypes.h"

#include <vector>

SwgCuiPerformanceScriptPicker::SwgCuiPerformanceScriptPicker(UIPage &page) :
	CuiMediator("SwgCuiPerformanceScriptPicker", page),
	UIEventCallback(),
	m_scriptCombo(0),
	m_statusText(0),
	m_refreshButton(0),
	m_startButton(0),
	m_cancelButton(0)
{
	getCodeDataObject(TUIComboBox, m_scriptCombo, "script");
	getCodeDataObject(TUIText, m_statusText, "status");
	getCodeDataObject(TUIButton, m_refreshButton, "refresh");
	getCodeDataObject(TUIButton, m_startButton, "start");
	getCodeDataObject(TUIButton, m_cancelButton, "cancel");

	m_statusText->SetPreLocalized(true);
	registerMediatorObject(*m_scriptCombo, true);
	registerMediatorObject(*m_refreshButton, true);
	registerMediatorObject(*m_startButton, true);
	registerMediatorObject(*m_cancelButton, true);
	setState(MS_closeDeactivates);
	setShowFocusedGlowRect(false);
}

SwgCuiPerformanceScriptPicker::~SwgCuiPerformanceScriptPicker()
{
}

void SwgCuiPerformanceScriptPicker::performActivate()
{
	refreshScripts();
	CuiManager::requestPointer(true);
}

void SwgCuiPerformanceScriptPicker::performDeactivate()
{
	CuiManager::requestPointer(false);
}

void SwgCuiPerformanceScriptPicker::OnButtonPressed(UIWidget *context)
{
	if (context == m_cancelButton)
	{
		closeThroughWorkspace();
		return;
	}
	if (context == m_refreshButton)
	{
		refreshScripts();
		return;
	}
	if (context != m_startButton)
		return;

	std::string fileName;
	m_scriptCombo->GetSelectedIndexName(fileName);
	std::string statusMessage;
	if (PerformanceModeManager::startMusicFromScript(fileName, statusMessage))
	{
		IGNORE_RETURN(CuiMediatorFactory::activateInWorkspace(CuiMediatorTypes::WS_PerformanceHud));
		closeThroughWorkspace();
	}
	else
		m_statusText->SetLocalText(Unicode::narrowToWide(statusMessage));
}

void SwgCuiPerformanceScriptPicker::refreshScripts()
{
	std::vector<std::string> const scripts = PerformanceModeManager::getAvailableMidiScripts();
	m_scriptCombo->Clear();
	for (std::vector<std::string>::const_iterator i = scripts.begin(); i != scripts.end(); ++i)
		m_scriptCombo->AddItem(Unicode::narrowToWide(*i), *i);

	bool const hasScripts = !scripts.empty();
	if (hasScripts)
	{
		m_scriptCombo->SetSelectedIndex(0);
		m_statusText->SetLocalText(Unicode::narrowToWide("Select a MIDI sequence from " + PerformanceModeManager::getMidiDirectory()));
	}
	else
		m_statusText->SetLocalText(Unicode::narrowToWide("Place a .mid or .midi file in " + PerformanceModeManager::getMidiDirectory() + " and select Refresh."));
	m_startButton->SetEnabled(hasScripts);
}
