#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiPerformanceSongPicker.h"

#include "UIButton.h"
#include "UIComboBox.h"
#include "UIPage.h"
#include "UIText.h"
#include "UnicodeUtils.h"
#include "clientGame/PerformanceModeManager.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "clientUserInterface/CuiSkillManager.h"
#include "swgClientUserInterface/SwgCuiMediatorTypes.h"

#include <algorithm>
#include <utility>
#include <vector>

SwgCuiPerformanceSongPicker::SwgCuiPerformanceSongPicker(UIPage &page) :
	CuiMediator("SwgCuiPerformanceSongPicker", page),
	UIEventCallback(),
	m_songCombo(0),
	m_statusText(0),
	m_startButton(0),
	m_cancelButton(0)
{
	getCodeDataObject(TUIComboBox, m_songCombo, "song");
	getCodeDataObject(TUIText, m_statusText, "status");
	getCodeDataObject(TUIButton, m_startButton, "start");
	getCodeDataObject(TUIButton, m_cancelButton, "cancel");

	m_statusText->SetPreLocalized(true);
	registerMediatorObject(*m_songCombo, true);
	registerMediatorObject(*m_startButton, true);
	registerMediatorObject(*m_cancelButton, true);
	setState(MS_closeDeactivates);
	setShowFocusedGlowRect(false);
}

SwgCuiPerformanceSongPicker::~SwgCuiPerformanceSongPicker()
{
}

void SwgCuiPerformanceSongPicker::performActivate()
{
	refreshSongs();
	CuiManager::requestPointer(true);
}

void SwgCuiPerformanceSongPicker::performDeactivate()
{
	CuiManager::requestPointer(false);
}

void SwgCuiPerformanceSongPicker::OnButtonPressed(UIWidget *context)
{
	if (context == m_cancelButton)
	{
		closeThroughWorkspace();
		return;
	}
	if (context != m_startButton)
		return;

	std::string songName;
	m_songCombo->GetSelectedIndexName(songName);
	std::string statusMessage;
	if (PerformanceModeManager::startMidiToFlourish(songName, statusMessage))
	{
		IGNORE_RETURN(CuiMediatorFactory::activateInWorkspace(CuiMediatorTypes::WS_PerformanceHud));
		closeThroughWorkspace();
	}
	else
		m_statusText->SetLocalText(Unicode::narrowToWide(statusMessage));
}

void SwgCuiPerformanceSongPicker::refreshSongs()
{
	typedef std::pair<Unicode::String, std::string> SongEntry;
	std::vector<SongEntry> entries;
	std::vector<std::string> const songs = PerformanceModeManager::getAvailableMusicSongs();
	for (std::vector<std::string>::const_iterator i = songs.begin(); i != songs.end(); ++i)
	{
		Unicode::String displayName;
		std::string const commandName = Unicode::toLower("startMusic+" + *i);
		if (!CuiSkillManager::localizeCmdName(commandName, displayName) || displayName.empty())
			displayName = Unicode::narrowToWide(*i);
		entries.push_back(std::make_pair(displayName, *i));
	}
	std::sort(entries.begin(), entries.end());

	m_songCombo->Clear();
	for (std::vector<SongEntry>::const_iterator i = entries.begin(); i != entries.end(); ++i)
		m_songCombo->AddItem(i->first, i->second);

	bool const hasSongs = !entries.empty();
	if (hasSongs)
	{
		m_songCombo->SetSelectedIndex(0);
		m_statusText->SetLocalText(Unicode::narrowToWide("Select a song learned by this character."));
	}
	else
		m_statusText->SetLocalText(Unicode::narrowToWide("This character has not learned any music songs."));
	m_startButton->SetEnabled(hasSongs);
}
