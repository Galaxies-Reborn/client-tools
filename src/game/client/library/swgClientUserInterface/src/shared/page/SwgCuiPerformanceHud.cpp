#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiPerformanceHud.h"

#include "UIButton.h"
#include "UIPage.h"
#include "UIText.h"
#include "UnicodeUtils.h"
#include "clientGame/PerformanceModeManager.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "clientUserInterface/CuiManager.h"
#include "swgClientUserInterface/SwgCuiMediatorTypes.h"

#include <cstdio>

SwgCuiPerformanceHud::SwgCuiPerformanceHud(UIPage &page) :
	CuiMediator("SwgCuiPerformanceHud", page),
	UIEventCallback(),
	m_modeText(0),
	m_songText(0),
	m_deviceText(0),
	m_octaveText(0),
	m_settingsButton(0),
	m_stopButton(0),
	m_updateTimer(0.0f)
{
	getCodeDataObject(TUIText, m_modeText, "mode");
	getCodeDataObject(TUIText, m_songText, "song");
	getCodeDataObject(TUIText, m_deviceText, "device");
	getCodeDataObject(TUIText, m_octaveText, "octave");
	getCodeDataObject(TUIButton, m_settingsButton, "settings");
	getCodeDataObject(TUIButton, m_stopButton, "stop");

	m_modeText->SetPreLocalized(true);
	m_songText->SetPreLocalized(true);
	m_deviceText->SetPreLocalized(true);
	m_octaveText->SetPreLocalized(true);
	registerMediatorObject(*m_settingsButton, true);
	registerMediatorObject(*m_stopButton, true);

	for (int i = 0; i < 8; ++i)
	{
		char name[24];
		snprintf(name, sizeof(name), "flourish%d", i + 1);
		m_flourishButtons[i] = 0;
		getCodeDataObject(TUIButton, m_flourishButtons[i], name);
		registerMediatorObject(*m_flourishButtons[i], true);
	}

	setState(MS_closeDeactivates);
	setShowFocusedGlowRect(false);
}

SwgCuiPerformanceHud::~SwgCuiPerformanceHud()
{
}

void SwgCuiPerformanceHud::performActivate()
{
	if (!PerformanceModeManager::isActive())
	{
		deactivate();
		return;
	}
	setIsUpdating(true);
	CuiManager::requestPointer(true);
	m_updateTimer = 0.0f;
	updateDisplay();
}

void SwgCuiPerformanceHud::performDeactivate()
{
	setIsUpdating(false);
	CuiManager::requestPointer(false);
}

void SwgCuiPerformanceHud::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);
	if (!PerformanceModeManager::isActive())
	{
		closeThroughWorkspace();
		return;
	}

	m_updateTimer -= deltaTimeSecs;
	if (m_updateTimer <= 0.0f)
	{
		m_updateTimer = 0.25f;
		updateDisplay();
	}
}

void SwgCuiPerformanceHud::OnButtonPressed(UIWidget *context)
{
	if (context == m_stopButton)
	{
		PerformanceModeManager::stopPerformanceMode(true);
		closeThroughWorkspace();
		return;
	}
	if (context == m_settingsButton)
	{
		IGNORE_RETURN(CuiMediatorFactory::activateInWorkspace(CuiMediatorTypes::WS_Opt));
		return;
	}
	for (int i = 0; i < 8; ++i)
		if (context == m_flourishButtons[i])
		{
			IGNORE_RETURN(PerformanceModeManager::triggerFlourish(i + 1));
			return;
		}
}

void SwgCuiPerformanceHud::updateDisplay()
{
	m_modeText->SetLocalText(Unicode::narrowToWide("MIDI TO FLOURISH"));
	m_songText->SetLocalText(Unicode::narrowToWide(PerformanceModeManager::getSongName()));
	std::string const &device = PerformanceModeManager::getMidiDeviceName();
	m_deviceText->SetLocalText(Unicode::narrowToWide(device.empty() ? "Keyboard input" : device));
	char octave[32];
	snprintf(octave, sizeof(octave), "Octave %+d", PerformanceModeManager::getMidiOctaveShift());
	m_octaveText->SetLocalText(Unicode::narrowToWide(octave));
}
