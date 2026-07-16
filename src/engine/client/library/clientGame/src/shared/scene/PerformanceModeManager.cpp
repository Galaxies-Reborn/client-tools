#include "clientGame/FirstClientGame.h"
#include "clientGame/PerformanceModeManager.h"

#include "clientGame/ClientCommandQueue.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "clientGame/PlayerMusicManager.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedFoundation/Os.h"
#include "sharedIoWin/IoWin.h"
#include "sharedUtility/CurrentUserOptionManager.h"
#include "sharedUtility/LocalMachineOptionManager.h"

#if defined(_WIN64)
#include "clientAudio/ClientAudioMidi.h"
#endif

#include "UnicodeUtils.h"
#include "PerformanceMidiMessage.h"

#include <algorithm>
#include <cstdio>
#include <dinput.h>

namespace PerformanceModeManagerNamespace
{
	bool s_installed = false;
	PerformanceModeManager::Mode s_mode = PerformanceModeManager::M_none;
	PerformanceModeManager::Mode s_pendingMode = PerformanceModeManager::M_none;
	std::string s_songName;
	std::string s_statusMessage;
	std::string s_midiDeviceName;
	std::string s_midiDeviceIdentifier;
	std::string s_instrumentName;
	std::string s_scriptFileName;
	int s_instrumentId = 0;
	double s_scriptPositionSeconds = 0.0;
	double s_scriptDurationSeconds = 0.0;
	float s_scriptEventBudget = 0.0f;
	bool s_scriptPaused = false;
#if defined(_WIN64)
	std::vector<ClientAudioMidiEvent> s_scriptEvents;
	size_t s_scriptEventIndex = 0;
#endif
	float s_flourishCooldown = 0.0f;
	float s_pendingTimeout = 0.0f;
	bool s_keyHeld[8] = {false, false, false, false, false, false, false, false};
	bool s_musicKeyHeld[24] = {};
	int s_musicKeyHeldNote[24] = {};
	int s_activeMidiNotes[16][128] = {};
	bool s_octaveKeyHeld[2] = {false, false};

	int const s_defaultFlourishMidiNotes[8] = {60, 61, 62, 63, 64, 65, 66, 67};
	int const s_defaultFlourishKeys[8] = {DIK_Q, DIK_W, DIK_E, DIK_R, DIK_T, DIK_Y, DIK_U, DIK_I};
	int s_flourishMidiNotes[8] = {60, 61, 62, 63, 64, 65, 66, 67};
	int s_flourishKeys[8] = {DIK_Q, DIK_W, DIK_E, DIK_R, DIK_T, DIK_Y, DIK_U, DIK_I};
	int const s_defaultMusicKeys[24] =
	{
		DIK_Z, DIK_S, DIK_X, DIK_D, DIK_C, DIK_V, DIK_G, DIK_B, DIK_H, DIK_N, DIK_J, DIK_M,
		DIK_COMMA, DIK_L, DIK_PERIOD, DIK_SEMICOLON, DIK_SLASH, DIK_Q, DIK_2, DIK_W, DIK_3, DIK_E, DIK_R, DIK_5
	};
	int s_musicKeys[24] =
	{
		DIK_Z, DIK_S, DIK_X, DIK_D, DIK_C, DIK_V, DIK_G, DIK_B, DIK_H, DIK_N, DIK_J, DIK_M,
		DIK_COMMA, DIK_L, DIK_PERIOD, DIK_SEMICOLON, DIK_SLASH, DIK_Q, DIK_2, DIK_W, DIK_3, DIK_E, DIK_R, DIK_5
	};
	int s_midiOctaveShift = 0;
	float const s_minimumFlourishInterval = 0.12f;
	float const s_serverConfirmationTimeout = 8.0f;
	int const s_optionVersion = 1;
	char const * const s_userOptionSection = "ClientGame/EntertainerReborn";
	char const * const s_machineOptionSection = "ClientGame/EntertainerRebornMidi";
	float const s_scriptEventsPerSecond = 160.0f;

	bool isSynthMode(PerformanceModeManager::Mode mode)
	{
		return mode == PerformanceModeManager::M_midiToMusic || mode == PerformanceModeManager::M_musicFromScript;
	}

	std::string midiDirectoryPath()
	{
		std::string path = Os::getProgramStartupDirectory() ? Os::getProgramStartupDirectory() : ".";
		while (!path.empty() && (path[path.size() - 1] == '\\' || path[path.size() - 1] == '/'))
			path.erase(path.size() - 1);
		return path + "\\midi";
	}

	bool ensureMidiDirectory(std::string &errorMessage)
	{
		std::string const path = midiDirectoryPath();
		DWORD const attributes = GetFileAttributesA(path.c_str());
		if (attributes != INVALID_FILE_ATTRIBUTES)
		{
			if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
			{
				errorMessage = "The client midi path must be a normal directory: " + path;
				return false;
			}
			return true;
		}
		if (!CreateDirectoryA(path.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		{
			errorMessage = "Could not create the client midi directory: " + path;
			return false;
		}
		return true;
	}

	bool isMidiFileName(std::string const &fileName)
	{
		if (fileName.empty() || fileName.size() > 180 || fileName.find_first_of("\\/:") != std::string::npos)
			return false;
		size_t const extension = fileName.find_last_of('.');
		if (extension == std::string::npos)
			return false;
		char const * const value = fileName.c_str() + extension;
		return _stricmp(value, ".mid") == 0 || _stricmp(value, ".midi") == 0;
	}

	bool resolveMidiFile(std::string const &fileName, std::string &path, std::string &errorMessage)
	{
		if (!isMidiFileName(fileName))
		{
			errorMessage = "Choose a .mid or .midi file directly from the client midi directory";
			return false;
		}
		if (!ensureMidiDirectory(errorMessage))
			return false;
		path = midiDirectoryPath() + "\\" + fileName;
		DWORD const attributes = GetFileAttributesA(path.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			errorMessage = "MIDI file not found: " + fileName;
			return false;
		}
		if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			errorMessage = "Linked MIDI files are not allowed";
			return false;
		}
		return true;
	}

	void resetHeldKeys()
	{
		for (int i = 0; i < 8; ++i)
			s_keyHeld[i] = false;
		for (int i = 0; i < 24; ++i)
		{
			s_musicKeyHeld[i] = false;
			s_musicKeyHeldNote[i] = -1;
		}
		for (int channel = 0; channel < 16; ++channel)
			for (int note = 0; note < 128; ++note)
				s_activeMidiNotes[channel][note] = -1;
		s_octaveKeyHeld[0] = false;
		s_octaveKeyHeld[1] = false;
	}

	int findKeyFlourish(int key)
	{
		for (int i = 0; i < 8; ++i)
			if (s_flourishKeys[i] == key)
				return i + 1;
		return 0;
	}

	int findMidiFlourish(int note)
	{
		for (int i = 0; i < 8; ++i)
			if (s_flourishMidiNotes[i] + s_midiOctaveShift * 12 == note)
				return i + 1;
		return 0;
	}

	int findMusicKey(int key)
	{
		for (int i = 0; i < 24; ++i)
			if (s_musicKeys[i] == key)
				return i;
		return -1;
	}

	void validateOptions()
	{
		for (int i = 0; i < 8; ++i)
		{
			if (s_flourishKeys[i] < 0 || s_flourishKeys[i] > 255)
				s_flourishKeys[i] = s_defaultFlourishKeys[i];
			if (s_flourishMidiNotes[i] < 0 || s_flourishMidiNotes[i] > 127)
				s_flourishMidiNotes[i] = s_defaultFlourishMidiNotes[i];
		}
		for (int i = 0; i < 24; ++i)
			if (s_musicKeys[i] < 0 || s_musicKeys[i] > 255)
				s_musicKeys[i] = s_defaultMusicKeys[i];
		s_midiOctaveShift = std::max(-4, std::min(4, s_midiOctaveShift));
	}

	void closeMidiInput()
	{
#if defined(_WIN64)
		ClientAudioMidi::closeInput();
#endif
	}

	void clearModeState()
	{
#if defined(_WIN64)
		CreatureObject const * const player = Game::getPlayerCreature();
		if (player && (isSynthMode(s_mode) || isSynthMode(s_pendingMode)))
			PlayerMusicManager::endMidiPerformance(player);
		s_scriptEvents.clear();
		s_scriptEventIndex = 0;
#endif
		closeMidiInput();
		s_mode = PerformanceModeManager::M_none;
		s_pendingMode = PerformanceModeManager::M_none;
		s_songName.clear();
		s_pendingTimeout = 0.0f;
		s_flourishCooldown = 0.0f;
		s_instrumentId = 0;
		s_instrumentName.clear();
		s_scriptFileName.clear();
		s_scriptPositionSeconds = 0.0;
		s_scriptDurationSeconds = 0.0;
		s_scriptEventBudget = 0.0f;
		s_scriptPaused = false;
		resetHeldKeys();
	}

	void sendMidiMessage(PerformanceMidiMessage::Type type, int channel, int note, int value)
	{
		char parameter[32];
		snprintf(parameter, sizeof(parameter), "%d", PerformanceMidiMessage::encode(type, channel, note, value));
		ClientCommandQueue::sendImmediateCommandToServer("performanceMidiEvent", NetworkId::cms_invalid, Unicode::narrowToWide(parameter));
	}

	void playLocalNote(int channel, int note, int velocity)
	{
#if defined(_WIN64)
		CreatureObject const * const player = Game::getPlayerCreature();
		if (player)
			ClientAudioMidi::synthNoteOn(static_cast<unsigned long long>(player->getNetworkId().getValue()), channel, note, velocity);
#else
		UNREF(channel);
		UNREF(note);
		UNREF(velocity);
#endif
		sendMidiMessage(PerformanceMidiMessage::T_noteOn, channel, note, velocity);
	}

	void releaseLocalNote(int channel, int note)
	{
#if defined(_WIN64)
		CreatureObject const * const player = Game::getPlayerCreature();
		if (player)
			ClientAudioMidi::synthNoteOff(static_cast<unsigned long long>(player->getNetworkId().getValue()), channel, note);
#else
		UNREF(channel);
		UNREF(note);
#endif
		sendMidiMessage(PerformanceMidiMessage::T_noteOff, channel, note, 0);
	}

	void sendAllNotesOff()
	{
#if defined(_WIN64)
		CreatureObject const * const player = Game::getPlayerCreature();
		if (player)
			ClientAudioMidi::synthAllNotesOff(static_cast<unsigned long long>(player->getNetworkId().getValue()));
#endif
		sendMidiMessage(PerformanceMidiMessage::T_allNotesOff, 0, 0, 0);
		for (int channel = 0; channel < 16; ++channel)
			for (int note = 0; note < 128; ++note)
				s_activeMidiNotes[channel][note] = -1;
	}

#if defined(_WIN64)
	void dispatchMidiEvent(ClientAudioMidiEvent const &event)
	{
		CreatureObject const * const player = Game::getPlayerCreature();
		if (!player)
			return;

		int const channel = std::max(0, std::min(15, event.channel - 1));
		int const sourceNote = std::max(0, std::min(127, event.note));
		if (event.type == ClientAudioMidiEvent::T_noteOn && event.value > 0)
		{
			int const shiftedNote = std::max(0, std::min(127, sourceNote + s_midiOctaveShift * 12));
			int const activeNote = s_activeMidiNotes[channel][sourceNote];
			if (activeNote >= 0 && s_mode != PerformanceModeManager::M_musicFromScript)
				releaseLocalNote(channel, activeNote);
			s_activeMidiNotes[channel][sourceNote] = shiftedNote;
			playLocalNote(channel, shiftedNote, event.value);
		}
		else if (event.type == ClientAudioMidiEvent::T_noteOff || (event.type == ClientAudioMidiEvent::T_noteOn && event.value == 0))
		{
			int const activeNote = s_activeMidiNotes[channel][sourceNote];
			if (activeNote >= 0)
			{
				releaseLocalNote(channel, activeNote);
				s_activeMidiNotes[channel][sourceNote] = -1;
			}
		}
		else if (event.type == ClientAudioMidiEvent::T_controlChange && event.note == 64)
		{
			unsigned long long const performerId = static_cast<unsigned long long>(player->getNetworkId().getValue());
			ClientAudioMidi::synthSustain(performerId, channel, event.value >= 64);
			sendMidiMessage(PerformanceMidiMessage::T_sustain, channel, 0, event.value);
		}
		else if (event.type == ClientAudioMidiEvent::T_controlChange && (event.note == 120 || event.note == 123))
			sendAllNotesOff();
	}
#endif

	bool activatePendingMode()
	{
		s_mode = s_pendingMode;
		s_pendingMode = PerformanceModeManager::M_none;
		s_pendingTimeout = 0.0f;
		s_flourishCooldown = 0.0f;
		resetHeldKeys();

#if defined(_WIN64)
		std::string midiStatus;
		bool midiOpened = false;
		if (s_mode != PerformanceModeManager::M_musicFromScript)
		{
			if (!s_midiDeviceIdentifier.empty())
				midiOpened = ClientAudioMidi::openInput(s_midiDeviceIdentifier, midiStatus);
			if (!midiOpened)
			{
				midiOpened = ClientAudioMidi::openFirstAvailableInput(s_midiDeviceName, midiStatus);
				if (midiOpened)
					s_midiDeviceIdentifier = ClientAudioMidi::getOpenInputIdentifier();
			}
		}
		else
		{
			ClientAudioMidi::closeInput();
			ClientAudioMidi::clearEvents();
		}
		if (isSynthMode(s_mode))
		{
			CreatureObject const * const player = Game::getPlayerCreature();
			s_instrumentId = PlayerMusicManager::getInstrumentId(player);
			if (!player || s_instrumentId < 1 || s_instrumentId > 14)
			{
				clearModeState();
				s_statusMessage = "The server started music without a supported instrument patch.";
				return false;
			}
			s_instrumentName = ClientAudioMidi::getSynthPatchName(s_instrumentId);
			PlayerMusicManager::beginMidiPerformance(player, s_instrumentId);
			sendMidiMessage(PerformanceMidiMessage::T_sessionStart, 0, 0, s_instrumentId);
			if (s_mode == PerformanceModeManager::M_musicFromScript)
			{
				s_scriptPositionSeconds = 0.0;
				s_scriptEventIndex = 0;
				s_scriptEventBudget = s_scriptEventsPerSecond;
				s_scriptPaused = false;
				s_statusMessage = "Playing " + s_scriptFileName + " with " + s_instrumentName + ".";
			}
			else if (midiOpened)
				s_statusMessage = "Midi to Music active: " + s_instrumentName + " with " + s_midiDeviceName + ".";
			else
				s_statusMessage = "Midi to Music active: " + s_instrumentName + ". " + midiStatus + ".";
		}
		else if (midiOpened)
			s_statusMessage = "Midi to Flourish active with " + s_midiDeviceName + ".";
		else
			s_statusMessage = "Midi to Flourish active. " + midiStatus + ". Keyboard input remains available.";
#else
		s_midiDeviceName.clear();
		s_statusMessage = s_mode == PerformanceModeManager::M_midiToFlourish
			? "Midi to Flourish active. Keyboard input is available; MIDI requires the x64 client."
			: "This performance mode requires the x64 client.";
#endif
		return true;
	}
}

using namespace PerformanceModeManagerNamespace;

void PerformanceModeManager::install()
{
	InstallTimer const installTimer("PerformanceModeManager::install");
	DEBUG_FATAL(s_installed, ("PerformanceModeManager is already installed"));
	s_installed = true;
	char optionName[64];
	for (int i = 0; i < 8; ++i)
	{
		snprintf(optionName, sizeof(optionName), "flourishKey%d", i + 1);
		CurrentUserOptionManager::registerOption(s_flourishKeys[i], s_userOptionSection, optionName, s_optionVersion);
		snprintf(optionName, sizeof(optionName), "flourishMidiNote%d", i + 1);
		CurrentUserOptionManager::registerOption(s_flourishMidiNotes[i], s_userOptionSection, optionName, s_optionVersion);
	}
	for (int i = 0; i < 24; ++i)
	{
		snprintf(optionName, sizeof(optionName), "musicKey%d", i + 1);
		CurrentUserOptionManager::registerOption(s_musicKeys[i], s_userOptionSection, optionName, s_optionVersion);
	}
	CurrentUserOptionManager::registerOption(s_midiOctaveShift, s_userOptionSection, "midiOctaveShift", s_optionVersion);
	LocalMachineOptionManager::registerOption(s_midiDeviceIdentifier, s_machineOptionSection, "inputIdentifier", s_optionVersion);
	validateOptions();
	std::string midiDirectoryError;
	if (!ensureMidiDirectory(midiDirectoryError))
		s_statusMessage = midiDirectoryError;

#if defined(_WIN64)
	std::vector<MidiDevice> const devices = getMidiDevices();
	for (std::vector<MidiDevice>::const_iterator i = devices.begin(); i != devices.end(); ++i)
		if (i->identifier == s_midiDeviceIdentifier)
		{
			s_midiDeviceName = i->name;
			break;
		}
#endif
	ExitChain::add(PerformanceModeManager::remove, "PerformanceModeManager::remove", 0, false);
}

void PerformanceModeManager::remove()
{
	if (!s_installed)
		return;
	stopPerformanceMode(false);
	s_installed = false;
}

void PerformanceModeManager::alter(float elapsedTime)
{
	if (!s_installed)
		return;

	CreatureObject const * const player = Game::getPlayerCreature();
	if (s_pendingMode != M_none)
	{
		if (player && player->getPerformanceType() > 0)
		{
			if (!activatePendingMode())
				return;
		}
		else
		{
			s_pendingTimeout -= elapsedTime;
			if (s_pendingTimeout <= 0.0f)
			{
				clearModeState();
				s_statusMessage = "Server did not start the selected performance. Check your instrument, skill, and current state.";
			}
			return;
		}
	}

	if (s_mode == M_none)
		return;

	if (!player || player->getPerformanceType() == 0)
	{
		clearModeState();
		s_statusMessage = "Performance ended.";
		return;
	}

	s_flourishCooldown = std::max(0.0f, s_flourishCooldown - elapsedTime);

#if defined(_WIN64)
	if (s_mode == M_musicFromScript)
	{
		if (!s_scriptPaused)
		{
			s_scriptPositionSeconds += std::max(0.0f, elapsedTime);
			s_scriptEventBudget = std::min(s_scriptEventsPerSecond, s_scriptEventBudget + std::max(0.0f, elapsedTime) * s_scriptEventsPerSecond);
			int dispatchBudget = 128;
			while (dispatchBudget-- > 0 && s_scriptEventBudget >= 1.0f && s_scriptEventIndex < s_scriptEvents.size() && s_scriptEvents[s_scriptEventIndex].timestampSeconds <= s_scriptPositionSeconds)
			{
				dispatchMidiEvent(s_scriptEvents[s_scriptEventIndex++]);
				s_scriptEventBudget -= 1.0f;
			}
		}
		if (s_scriptEventIndex >= s_scriptEvents.size() && s_scriptPositionSeconds >= s_scriptDurationSeconds + 0.05)
		{
			std::string const completedFile = s_scriptFileName;
			stopPerformanceMode(true);
			s_statusMessage = "Completed MIDI script: " + completedFile;
		}
		return;
	}

	ClientAudioMidiEvent event;
	int eventBudget = 128;
	while (eventBudget-- > 0 && ClientAudioMidi::pollEvent(event))
	{
		if (s_mode == M_midiToFlourish && event.type == ClientAudioMidiEvent::T_noteOn && event.value > 0)
		{
			int const flourish = findMidiFlourish(event.note);
			if (flourish != 0)
				triggerFlourish(flourish);
		}
		else if (s_mode == M_midiToMusic)
			dispatchMidiEvent(event);
	}
#endif
}

bool PerformanceModeManager::processEvent(IoEvent const &event)
{
	if (!s_installed || s_mode == M_none)
		return false;

	if (event.type == IOET_InputReset)
	{
		if (s_mode == M_midiToMusic)
		{
#if defined(_WIN64)
			CreatureObject const * const player = Game::getPlayerCreature();
			if (player)
				ClientAudioMidi::synthAllNotesOff(static_cast<unsigned long long>(player->getNetworkId().getValue()));
#endif
			sendMidiMessage(PerformanceMidiMessage::T_allNotesOff, 0, 0, 0);
		}
		resetHeldKeys();
		return false;
	}

	if (event.type != IOET_KeyDown && event.type != IOET_KeyUp)
		return false;

	if (event.type == IOET_KeyDown && event.arg2 == DIK_ESCAPE)
	{
		stopPerformanceMode(true);
		return true;
	}

	if (s_mode != M_midiToFlourish)
	{
		if (s_mode != M_midiToMusic)
			return false;

		if (event.arg2 == DIK_LBRACKET || event.arg2 == DIK_RBRACKET)
		{
			int const direction = event.arg2 == DIK_LBRACKET ? -1 : 1;
			int const index = direction < 0 ? 0 : 1;
			if (event.type == IOET_KeyUp)
				s_octaveKeyHeld[index] = false;
			else if (!s_octaveKeyHeld[index])
			{
				s_octaveKeyHeld[index] = true;
				setMidiOctaveShift(s_midiOctaveShift + direction);
			}
			return true;
		}

		int const musicKey = findMusicKey(event.arg2);
		if (musicKey < 0)
			return false;
		if (event.type == IOET_KeyUp)
		{
			if (s_musicKeyHeld[musicKey] && s_musicKeyHeldNote[musicKey] >= 0)
				releaseLocalNote(0, s_musicKeyHeldNote[musicKey]);
			s_musicKeyHeld[musicKey] = false;
			s_musicKeyHeldNote[musicKey] = -1;
			return true;
		}
		if (!s_musicKeyHeld[musicKey])
		{
			int const note = std::max(0, std::min(127, 48 + musicKey + s_midiOctaveShift * 12));
			s_musicKeyHeld[musicKey] = true;
			s_musicKeyHeldNote[musicKey] = note;
			playLocalNote(0, note, 100);
		}
		return true;
	}

	int const flourish = findKeyFlourish(event.arg2);
	if (flourish == 0)
		return false;

	int const index = flourish - 1;
	if (event.type == IOET_KeyUp)
	{
		s_keyHeld[index] = false;
		return true;
	}

	if (!s_keyHeld[index])
	{
		s_keyHeld[index] = true;
		triggerFlourish(flourish);
	}
	return true;
}

bool PerformanceModeManager::startMidiToFlourish(std::string const &songName, std::string &statusMessage)
{
	statusMessage.clear();
	if (!s_installed)
	{
		statusMessage = "The performance system is not installed";
		return false;
	}
	if (songName.empty())
	{
		statusMessage = "Choose a song: /startMidiToFlourish <song>";
		return false;
	}

	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
	{
		statusMessage = "Enter the game before starting a performance";
		return false;
	}
	if (hasPerformanceMode() || player->getPerformanceType() != 0)
	{
		statusMessage = "Stop the current performance before starting Midi to Flourish";
		return false;
	}

	std::vector<std::string> const songs = getAvailableMusicSongs();
	std::string selectedSong;
	for (std::vector<std::string>::const_iterator i = songs.begin(); i != songs.end(); ++i)
		if (_stricmp(i->c_str(), songName.c_str()) == 0)
		{
			selectedSong = *i;
			break;
		}
	if (selectedSong.empty())
	{
		statusMessage = "That character has not learned the selected music song";
		return false;
	}

	IGNORE_RETURN(ClientCommandQueue::enqueueCommand("startMusic", NetworkId::cms_invalid, Unicode::narrowToWide(selectedSong)));
	s_pendingMode = M_midiToFlourish;
	s_songName = selectedSong;
	s_pendingTimeout = s_serverConfirmationTimeout;
	s_flourishCooldown = 0.0f;
	resetHeldKeys();
	s_statusMessage = "Waiting for the server to start " + selectedSong + "...";
	statusMessage = s_statusMessage;
	return true;
}

bool PerformanceModeManager::startMidiToMusic(std::string &statusMessage)
{
	statusMessage.clear();
	if (!s_installed)
	{
		statusMessage = "The performance system is not installed";
		return false;
	}

	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
	{
		statusMessage = "Enter the game before starting a performance";
		return false;
	}
	if (hasPerformanceMode() || player->getPerformanceType() != 0)
	{
		statusMessage = "Stop the current performance before starting Midi to Music";
		return false;
	}

	std::vector<std::string> const songs = getAvailableMusicSongs();
	if (songs.empty())
	{
		statusMessage = "This character has not learned a music song";
		return false;
	}

	s_songName = songs.front();
	IGNORE_RETURN(ClientCommandQueue::enqueueCommand("startMusic", NetworkId::cms_invalid, Unicode::narrowToWide(s_songName)));
	s_pendingMode = M_midiToMusic;
	s_pendingTimeout = s_serverConfirmationTimeout;
	resetHeldKeys();
	s_statusMessage = "Waiting for the server to authorize Midi to Music...";
	statusMessage = s_statusMessage;
	return true;
}

bool PerformanceModeManager::startMusicFromScript(std::string const &fileName, std::string &statusMessage)
{
	statusMessage.clear();
#if !defined(_WIN64)
	UNREF(fileName);
	statusMessage = "Music from Script requires the x64 client";
	return false;
#else
	if (!s_installed)
	{
		statusMessage = "The performance system is not installed";
		return false;
	}
	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
	{
		statusMessage = "Enter the game before starting a performance";
		return false;
	}
	if (hasPerformanceMode() || player->getPerformanceType() != 0)
	{
		statusMessage = "Stop the current performance before starting Music from Script";
		return false;
	}

	std::string filePath;
	if (!resolveMidiFile(fileName, filePath, statusMessage))
		return false;
	std::vector<ClientAudioMidiEvent> events;
	ClientAudioMidiSequenceInfo info = {};
	if (!ClientAudioMidi::loadMidiSequence(filePath, events, info, statusMessage))
		return false;

	std::vector<std::string> const songs = getAvailableMusicSongs();
	if (songs.empty())
	{
		statusMessage = "This character has not learned a music song";
		return false;
	}

	s_scriptEvents.swap(events);
	s_scriptEventIndex = 0;
	s_scriptFileName = fileName;
	s_scriptPositionSeconds = 0.0;
	s_scriptDurationSeconds = info.durationSeconds;
	s_scriptEventBudget = s_scriptEventsPerSecond;
	s_scriptPaused = false;
	s_songName = songs.front();
	IGNORE_RETURN(ClientCommandQueue::enqueueCommand("startMusic", NetworkId::cms_invalid, Unicode::narrowToWide(s_songName)));
	s_pendingMode = M_musicFromScript;
	s_pendingTimeout = s_serverConfirmationTimeout;
	resetHeldKeys();
	char details[256];
	snprintf(details, sizeof(details), "Waiting for the server to authorize %s (format %d, %d tracks, %d events, %.1f seconds)...", fileName.c_str(), info.fileType, info.trackCount, info.eventCount, info.durationSeconds);
	s_statusMessage = details;
	statusMessage = s_statusMessage;
	return true;
#endif
}

void PerformanceModeManager::stopPerformanceMode(bool sendServerStop)
{
	if (!hasPerformanceMode())
		return;

	CreatureObject const * const player = Game::getPlayerCreature();
	if (isSynthMode(s_mode) && player)
	{
#if defined(_WIN64)
		unsigned long long const performerId = static_cast<unsigned long long>(player->getNetworkId().getValue());
		ClientAudioMidi::synthAllNotesOff(performerId);
#endif
		if (sendServerStop)
		{
			sendMidiMessage(PerformanceMidiMessage::T_allNotesOff, 0, 0, 0);
			sendMidiMessage(PerformanceMidiMessage::T_sessionStop, 0, 0, 0);
		}
		PlayerMusicManager::endMidiPerformance(player);
	}

	if (sendServerStop)
		IGNORE_RETURN(ClientCommandQueue::enqueueCommand("stopMusic", NetworkId::cms_invalid, Unicode::emptyString));

	clearModeState();
	s_statusMessage = "Performance mode stopped.";
}

bool PerformanceModeManager::triggerFlourish(int flourishNumber)
{
	if (s_mode != M_midiToFlourish || flourishNumber < 1 || flourishNumber > 8 || s_flourishCooldown > 0.0f)
		return false;

	char parameter[4];
	snprintf(parameter, sizeof(parameter), "%d", flourishNumber);
	IGNORE_RETURN(ClientCommandQueue::enqueueCommand("flourish", NetworkId::cms_invalid, Unicode::narrowToWide(parameter)));
	s_flourishCooldown = s_minimumFlourishInterval;
	return true;
}

bool PerformanceModeManager::toggleScriptPaused()
{
	if (s_mode != M_musicFromScript)
		return false;
	s_scriptPaused = !s_scriptPaused;
	if (s_scriptPaused)
	{
		sendAllNotesOff();
		s_statusMessage = "MIDI script paused: " + s_scriptFileName;
	}
	else
		s_statusMessage = "Playing MIDI script: " + s_scriptFileName;
	return true;
}

PerformanceModeManager::Mode PerformanceModeManager::getMode()
{
	return s_mode;
}

PerformanceModeManager::Mode PerformanceModeManager::getRequestedMode()
{
	return s_mode != M_none ? s_mode : s_pendingMode;
}

bool PerformanceModeManager::isActive()
{
	return s_mode != M_none;
}

bool PerformanceModeManager::isPending()
{
	return s_pendingMode != M_none;
}

bool PerformanceModeManager::hasPerformanceMode()
{
	return isActive() || isPending();
}

std::string const &PerformanceModeManager::getSongName()
{
	return s_songName;
}

std::string const &PerformanceModeManager::getStatusMessage()
{
	return s_statusMessage;
}

std::vector<std::string> PerformanceModeManager::getAvailableMusicSongs()
{
	std::vector<std::string> result;
	CreatureObject const * const player = Game::getPlayerCreature();
	if (!player)
		return result;

	std::string const prefix = "startmusic+";
	std::map<std::string, int> const &commands = player->getCommands();
	for (std::map<std::string, int>::const_iterator i = commands.begin(); i != commands.end(); ++i)
	{
		std::string const lowerCommand = Unicode::toLower(i->first);
		if (lowerCommand.compare(0, prefix.size(), prefix) == 0 && i->first.size() > prefix.size())
			result.push_back(i->first.substr(prefix.size()));
	}
	return result;
}

std::vector<std::string> PerformanceModeManager::getAvailableMidiScripts()
{
	std::vector<std::string> result;
	std::string errorMessage;
	if (!ensureMidiDirectory(errorMessage))
		return result;

	WIN32_FIND_DATAA data = {};
	std::string const pattern = midiDirectoryPath() + "\\*";
	HANDLE const search = FindFirstFileA(pattern.c_str(), &data);
	if (search == INVALID_HANDLE_VALUE)
		return result;
	do
	{
		std::string const fileName = data.cFileName;
		if ((data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0 && isMidiFileName(fileName))
			result.push_back(fileName);
	}
	while (FindNextFileA(search, &data));
	FindClose(search);
	std::sort(result.begin(), result.end(), [](std::string const &a, std::string const &b)
	{
		return _stricmp(a.c_str(), b.c_str()) < 0;
	});
	return result;
}

std::string PerformanceModeManager::getMidiDirectory()
{
	std::string errorMessage;
	IGNORE_RETURN(ensureMidiDirectory(errorMessage));
	return midiDirectoryPath();
}

std::string const &PerformanceModeManager::getMidiDeviceName()
{
	return s_midiDeviceName;
}

std::string const &PerformanceModeManager::getInstrumentName()
{
	return s_instrumentName;
}

std::string const &PerformanceModeManager::getScriptFileName()
{
	return s_scriptFileName;
}

double PerformanceModeManager::getScriptPositionSeconds()
{
	return s_scriptPositionSeconds;
}

double PerformanceModeManager::getScriptDurationSeconds()
{
	return s_scriptDurationSeconds;
}

bool PerformanceModeManager::isScriptPaused()
{
	return s_scriptPaused;
}

std::vector<PerformanceModeManager::MidiDevice> PerformanceModeManager::getMidiDevices()
{
	std::vector<MidiDevice> result;
#if defined(_WIN64)
	std::vector<ClientAudioMidiDevice> const devices = ClientAudioMidi::getInputDevices();
	for (std::vector<ClientAudioMidiDevice>::const_iterator i = devices.begin(); i != devices.end(); ++i)
	{
		MidiDevice device;
		device.name = i->name;
		device.identifier = i->identifier;
		result.push_back(device);
	}
#endif
	return result;
}

bool PerformanceModeManager::selectMidiDevice(std::string const &identifier, std::string &statusMessage)
{
#if defined(_WIN64)
	if (identifier.empty())
	{
		ClientAudioMidi::closeInput();
		s_midiDeviceName.clear();
		s_midiDeviceIdentifier.clear();
		statusMessage = "No MIDI input selected; performance keyboard remains available";
		return true;
	}
	std::vector<MidiDevice> const devices = getMidiDevices();
	for (std::vector<MidiDevice>::const_iterator i = devices.begin(); i != devices.end(); ++i)
	{
		if (i->identifier == identifier)
		{
			if (s_mode != M_none && !ClientAudioMidi::openInput(identifier, statusMessage))
				return false;
			s_midiDeviceName = i->name;
			s_midiDeviceIdentifier = i->identifier;
			statusMessage = "Selected MIDI input: " + s_midiDeviceName;
			return true;
		}
	}
	statusMessage = "The selected MIDI input is no longer available";
#else
	UNREF(identifier);
	statusMessage = "MIDI input requires the x64 client";
#endif
	return false;
}

int PerformanceModeManager::getFlourishKey(int flourishIndex)
{
	return flourishIndex >= 0 && flourishIndex < 8 ? s_flourishKeys[flourishIndex] : 0;
}

void PerformanceModeManager::setFlourishKey(int flourishIndex, int scanCode)
{
	if (flourishIndex >= 0 && flourishIndex < 8 && scanCode >= 0 && scanCode <= 255)
		s_flourishKeys[flourishIndex] = scanCode;
}

int PerformanceModeManager::getDefaultFlourishKey(int flourishIndex)
{
	return flourishIndex >= 0 && flourishIndex < 8 ? s_defaultFlourishKeys[flourishIndex] : 0;
}

int PerformanceModeManager::getFlourishMidiNote(int flourishIndex)
{
	return flourishIndex >= 0 && flourishIndex < 8 ? s_flourishMidiNotes[flourishIndex] : 60;
}

void PerformanceModeManager::setFlourishMidiNote(int flourishIndex, int midiNote)
{
	if (flourishIndex >= 0 && flourishIndex < 8 && midiNote >= 0 && midiNote <= 127)
		s_flourishMidiNotes[flourishIndex] = midiNote;
}

int PerformanceModeManager::getMusicKey(int noteIndex)
{
	return noteIndex >= 0 && noteIndex < 24 ? s_musicKeys[noteIndex] : 0;
}

void PerformanceModeManager::setMusicKey(int noteIndex, int scanCode)
{
	if (noteIndex >= 0 && noteIndex < 24 && scanCode >= 0 && scanCode <= 255)
		s_musicKeys[noteIndex] = scanCode;
}

int PerformanceModeManager::getDefaultMusicKey(int noteIndex)
{
	return noteIndex >= 0 && noteIndex < 24 ? s_defaultMusicKeys[noteIndex] : 0;
}

int PerformanceModeManager::getDefaultFlourishMidiNote(int flourishIndex)
{
	return flourishIndex >= 0 && flourishIndex < 8 ? s_defaultFlourishMidiNotes[flourishIndex] : 60;
}

int PerformanceModeManager::getMidiOctaveShift()
{
	return s_midiOctaveShift;
}

void PerformanceModeManager::setMidiOctaveShift(int octaveShift)
{
	s_midiOctaveShift = std::max(-4, std::min(4, octaveShift));
}

std::string const &PerformanceModeManager::getSelectedMidiDeviceIdentifier()
{
	return s_midiDeviceIdentifier;
}

void PerformanceModeManager::resetInputMappings()
{
	for (int i = 0; i < 8; ++i)
	{
		s_flourishKeys[i] = s_defaultFlourishKeys[i];
		s_flourishMidiNotes[i] = s_defaultFlourishMidiNotes[i];
	}
	for (int i = 0; i < 24; ++i)
		s_musicKeys[i] = s_defaultMusicKeys[i];
	s_midiOctaveShift = 0;
}
