#include "clientGame/FirstClientGame.h"
#include "clientGame/PerformanceModeManager.h"

#include "clientGame/ClientCommandQueue.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedIoWin/IoWin.h"
#include "sharedUtility/CurrentUserOptionManager.h"
#include "sharedUtility/LocalMachineOptionManager.h"

#if defined(_WIN64)
#include "clientAudio/ClientAudioMidi.h"
#endif

#include "UnicodeUtils.h"

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
	float s_flourishCooldown = 0.0f;
	float s_pendingTimeout = 0.0f;
	bool s_keyHeld[8] = {false, false, false, false, false, false, false, false};

	int const s_defaultFlourishMidiNotes[8] = {60, 61, 62, 63, 64, 65, 66, 67};
	int const s_defaultFlourishKeys[8] = {DIK_Q, DIK_W, DIK_E, DIK_R, DIK_T, DIK_Y, DIK_U, DIK_I};
	int s_flourishMidiNotes[8] = {60, 61, 62, 63, 64, 65, 66, 67};
	int s_flourishKeys[8] = {DIK_Q, DIK_W, DIK_E, DIK_R, DIK_T, DIK_Y, DIK_U, DIK_I};
	int s_midiOctaveShift = 0;
	float const s_minimumFlourishInterval = 0.12f;
	float const s_serverConfirmationTimeout = 8.0f;
	int const s_optionVersion = 1;
	char const * const s_userOptionSection = "ClientGame/EntertainerReborn";
	char const * const s_machineOptionSection = "ClientGame/EntertainerRebornMidi";

	void resetHeldKeys()
	{
		for (int i = 0; i < 8; ++i)
			s_keyHeld[i] = false;
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

	void validateOptions()
	{
		for (int i = 0; i < 8; ++i)
		{
			if (s_flourishKeys[i] < 0 || s_flourishKeys[i] > 255)
				s_flourishKeys[i] = s_defaultFlourishKeys[i];
			if (s_flourishMidiNotes[i] < 0 || s_flourishMidiNotes[i] > 127)
				s_flourishMidiNotes[i] = s_defaultFlourishMidiNotes[i];
		}
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
		closeMidiInput();
		s_mode = PerformanceModeManager::M_none;
		s_pendingMode = PerformanceModeManager::M_none;
		s_songName.clear();
		s_pendingTimeout = 0.0f;
		s_flourishCooldown = 0.0f;
		resetHeldKeys();
	}

	void activatePendingMode()
	{
		s_mode = s_pendingMode;
		s_pendingMode = PerformanceModeManager::M_none;
		s_pendingTimeout = 0.0f;
		s_flourishCooldown = 0.0f;
		resetHeldKeys();

#if defined(_WIN64)
		std::string midiStatus;
		bool midiOpened = false;
		if (!s_midiDeviceIdentifier.empty())
			midiOpened = ClientAudioMidi::openInput(s_midiDeviceIdentifier, midiStatus);
		if (!midiOpened)
		{
			midiOpened = ClientAudioMidi::openFirstAvailableInput(s_midiDeviceName, midiStatus);
			if (midiOpened)
				s_midiDeviceIdentifier = ClientAudioMidi::getOpenInputIdentifier();
		}
		if (midiOpened)
			s_statusMessage = "Midi to Flourish active with " + s_midiDeviceName + ".";
		else
			s_statusMessage = "Midi to Flourish active. " + midiStatus + ". Keyboard input remains available.";
#else
		s_midiDeviceName.clear();
		s_statusMessage = "Midi to Flourish active. Keyboard input is available; MIDI requires the x64 client.";
#endif
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
	CurrentUserOptionManager::registerOption(s_midiOctaveShift, s_userOptionSection, "midiOctaveShift", s_optionVersion);
	LocalMachineOptionManager::registerOption(s_midiDeviceIdentifier, s_machineOptionSection, "inputIdentifier", s_optionVersion);
	validateOptions();

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
			activatePendingMode();
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
	}
#endif
}

bool PerformanceModeManager::processEvent(IoEvent const &event)
{
	if (!s_installed || s_mode == M_none)
		return false;

	if (event.type == IOET_InputReset)
	{
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
		return false;

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

void PerformanceModeManager::stopPerformanceMode(bool sendServerStop)
{
	if (!hasPerformanceMode())
		return;

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

PerformanceModeManager::Mode PerformanceModeManager::getMode()
{
	return s_mode;
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

std::string const &PerformanceModeManager::getMidiDeviceName()
{
	return s_midiDeviceName;
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
	s_midiOctaveShift = 0;
}
