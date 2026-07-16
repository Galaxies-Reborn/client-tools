#include "clientGame/FirstClientGame.h"
#include "clientGame/PerformanceModeManager.h"

#include "clientGame/ClientCommandQueue.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedIoWin/IoWin.h"

#if defined(_WIN64)
#include "clientAudio/ClientAudioMidi.h"
#endif

#include "UnicodeUtils.h"

#include <dinput.h>

namespace PerformanceModeManagerNamespace
{
	bool s_installed = false;
	PerformanceModeManager::Mode s_mode = PerformanceModeManager::M_none;
	std::string s_songName;
	std::string s_midiDeviceName;
	std::string s_midiDeviceIdentifier;
	float s_flourishCooldown = 0.0f;
	bool s_keyHeld[8] = {false, false, false, false, false, false, false, false};

	int const s_flourishMidiNotes[8] = {60, 61, 62, 63, 64, 65, 66, 67};
	int const s_flourishKeys[8] = {DIK_Q, DIK_W, DIK_E, DIK_R, DIK_T, DIK_Y, DIK_U, DIK_I};
	float const s_minimumFlourishInterval = 0.12f;

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
			if (s_flourishMidiNotes[i] == note)
				return i + 1;
		return 0;
	}
}

using namespace PerformanceModeManagerNamespace;

void PerformanceModeManager::install()
{
	InstallTimer const installTimer("PerformanceModeManager::install");
	DEBUG_FATAL(s_installed, ("PerformanceModeManager is already installed"));
	s_installed = true;
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
	if (!s_installed || s_mode == M_none)
		return;

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

	if (s_mode != M_none)
		stopPerformanceMode(true);

	IGNORE_RETURN(ClientCommandQueue::enqueueCommand("startMusic", NetworkId::cms_invalid, Unicode::narrowToWide(songName)));
	s_mode = M_midiToFlourish;
	s_songName = songName;
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
		statusMessage = "Midi to Flourish started with " + s_midiDeviceName + ". MIDI C4-G4 and keys Q-I trigger flourishes 1-8.";
	else
		statusMessage = "Midi to Flourish started. " + midiStatus + ". Keys Q-I trigger flourishes 1-8.";
#else
	s_midiDeviceName.clear();
	statusMessage = "Midi to Flourish started. Keys Q-I trigger flourishes 1-8; MIDI requires the x64 client.";
#endif
	return true;
}

void PerformanceModeManager::stopPerformanceMode(bool sendServerStop)
{
	if (s_mode == M_none)
		return;

#if defined(_WIN64)
	ClientAudioMidi::closeInput();
#endif
	if (sendServerStop)
		IGNORE_RETURN(ClientCommandQueue::enqueueCommand("stopMusic", NetworkId::cms_invalid, Unicode::emptyString));

	s_mode = M_none;
	s_songName.clear();
	s_flourishCooldown = 0.0f;
	resetHeldKeys();
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

std::string const &PerformanceModeManager::getSongName()
{
	return s_songName;
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
