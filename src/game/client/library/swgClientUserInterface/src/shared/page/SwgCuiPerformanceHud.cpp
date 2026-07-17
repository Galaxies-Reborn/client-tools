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
	m_pauseButton(0),
	m_settingsButton(0),
	m_stopButton(0),
	m_updateTimer(0.0f),
	m_failureTimer(0.0f)
{
	getCodeDataObject(TUIText, m_modeText, "mode");
	getCodeDataObject(TUIText, m_songText, "song");
	getCodeDataObject(TUIText, m_deviceText, "device");
	getCodeDataObject(TUIText, m_octaveText, "octave");
	getCodeDataObject(TUIButton, m_pauseButton, "pause");
	getCodeDataObject(TUIButton, m_settingsButton, "settings");
	getCodeDataObject(TUIButton, m_stopButton, "stop");

	m_modeText->SetPreLocalized(true);
	m_songText->SetPreLocalized(true);
	m_deviceText->SetPreLocalized(true);
	m_octaveText->SetPreLocalized(true);
	registerMediatorObject(*m_pauseButton, true);
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
	if (!PerformanceModeManager::hasPerformanceMode())
	{
		deactivate();
		return;
	}
	setIsUpdating(true);
	m_updateTimer = 0.0f;
	m_failureTimer = 0.0f;
	updateDisplay();
}

void SwgCuiPerformanceHud::performDeactivate()
{
	setIsUpdating(false);
}

void SwgCuiPerformanceHud::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);
	if (!PerformanceModeManager::hasPerformanceMode())
	{
		if (PerformanceModeManager::getStatusMessage().find("Server did not start") == 0)
		{
			if (m_failureTimer <= 0.0f)
			{
				m_failureTimer = 4.0f;
				updateDisplay();
			}
			m_failureTimer -= deltaTimeSecs;
			if (m_failureTimer <= 0.0f)
				closeThroughWorkspace();
		}
		else
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
	if (context == m_pauseButton)
	{
		IGNORE_RETURN(PerformanceModeManager::toggleScriptPaused());
		updateDisplay();
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
	bool const pending = PerformanceModeManager::isPending();
	bool const failed = !PerformanceModeManager::hasPerformanceMode();
	PerformanceModeManager::Mode const mode = PerformanceModeManager::getRequestedMode();
	bool const midiToMusic = mode == PerformanceModeManager::M_midiToMusic;
	bool const musicFromScript = mode == PerformanceModeManager::M_musicFromScript;
	m_modeText->SetLocalText(Unicode::narrowToWide(failed ? "START FAILED" : (pending ? "STARTING..." : (musicFromScript ? "MUSIC FROM SCRIPT" : (midiToMusic ? "MIDI TO MUSIC" : "MIDI TO FLOURISH")))));
	std::string const performanceName = musicFromScript
		? PerformanceModeManager::getScriptFileName()
		: (midiToMusic && !PerformanceModeManager::getInstrumentName().empty()
		? PerformanceModeManager::getInstrumentName()
		: PerformanceModeManager::getSongName());
	m_songText->SetLocalText(Unicode::narrowToWide(failed ? "Not started" : performanceName));
	std::string const &device = PerformanceModeManager::getMidiDeviceName();
	if (musicFromScript && !pending && !failed)
	{
		int const position = static_cast<int>(PerformanceModeManager::getScriptPositionSeconds());
		int const duration = static_cast<int>(PerformanceModeManager::getScriptDurationSeconds());
		char progress[96];
		snprintf(progress, sizeof(progress), "%s%d:%02d / %d:%02d", PerformanceModeManager::isScriptPaused() ? "Paused  " : "Playing  ", position / 60, position % 60, duration / 60, duration % 60);
		m_deviceText->SetLocalText(Unicode::narrowToWide(progress));
	}
	else
		m_deviceText->SetLocalText(Unicode::narrowToWide(failed ? "Server rejected or timed out" : (pending ? PerformanceModeManager::getStatusMessage() : (device.empty() ? "Keyboard input" : device))));
	char octave[32];
	snprintf(octave, sizeof(octave), "Octave %+d", PerformanceModeManager::getMidiOctaveShift());
	m_octaveText->SetLocalText(Unicode::narrowToWide(octave));
	for (int i = 0; i < 8; ++i)
	{
		m_flourishButtons[i]->SetEnabled(!pending && !failed);
		m_flourishButtons[i]->SetVisible(mode == PerformanceModeManager::M_midiToFlourish);
	}
	m_pauseButton->SetVisible(musicFromScript);
	m_pauseButton->SetEnabled(musicFromScript && !pending && !failed);
	m_pauseButton->SetLocalText(Unicode::narrowToWide(PerformanceModeManager::isScriptPaused() ? "Resume" : "Pause"));
	m_settingsButton->SetVisible(!musicFromScript);
}
