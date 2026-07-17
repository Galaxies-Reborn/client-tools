#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>
#include <mss.h>

#include "clientAudio/ClientAudioMidi.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
	using Clock = std::chrono::steady_clock;

	unsigned long long const sessionId = 1;
	std::atomic<bool> quitRequested(false);

	struct KeyBinding
	{
		int virtualKey;
		char const *label;
	};

	std::array<KeyBinding, 24> const keyboardBindings = {{
		{'Z', "Z"}, {'S', "S"}, {'X', "X"}, {'D', "D"}, {'C', "C"}, {'V', "V"},
		{'G', "G"}, {'B', "B"}, {'H', "H"}, {'N', "N"}, {'J', "J"}, {'M', "M"},
		{VK_OEM_COMMA, ","}, {'L', "L"}, {VK_OEM_PERIOD, "."}, {VK_OEM_1, ";"},
		{VK_OEM_2, "/"}, {'Q', "Q"}, {'2', "2"}, {'W', "W"}, {'3', "3"}, {'E', "E"},
		{'R', "R"}, {'5', "5"}
	}};

	std::string formatTime(double seconds)
	{
		seconds = (std::max)(0.0, seconds);
		int const whole = static_cast<int>(seconds);
		char value[32];
		snprintf(value, sizeof(value), "%d:%02d", whole / 60, whole % 60);
		return value;
	}

	std::string selectMidiFile(HWND owner)
	{
		char path[MAX_PATH] = {};
		OPENFILENAMEA dialog = {};
		dialog.lStructSize = sizeof(dialog);
		dialog.hwndOwner = owner;
		dialog.lpstrFilter = "MIDI files (*.mid;*.midi)\0*.mid;*.midi\0All files (*.*)\0*.*\0\0";
		dialog.lpstrFile = path;
		dialog.nMaxFile = static_cast<DWORD>(sizeof(path));
		dialog.lpstrTitle = "Choose a MIDI sequence";
		dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
		return GetOpenFileNameA(&dialog) ? path : std::string();
	}

	BOOL WINAPI consoleControlHandler(DWORD controlType)
	{
		if (controlType == CTRL_C_EVENT || controlType == CTRL_BREAK_EVENT || controlType == CTRL_CLOSE_EVENT)
		{
			quitRequested = true;
			return TRUE;
		}
		return FALSE;
	}

	class Harness
	{
	public:
		Harness() :
			driver(nullptr),
			instrumentId(1),
			octaveShift(0),
			midiDeviceIndex(-1),
			scriptEventIndex(0),
			scriptPosition(0.0),
			scriptDuration(0.0),
			scriptPlaying(false),
			scriptPaused(false),
			consoleInput(GetStdHandle(STD_INPUT_HANDLE)),
			originalConsoleMode(0),
			restoreConsoleMode(false)
		{
			keyboardNotes.fill(-1);
			for (std::array<int, 128> &channel : activeMidiNotes)
				channel.fill(-1);
		}

		~Harness()
		{
			stopScript(false);
			allNotesOff();
			ClientAudioMidi::closeInput();
			ClientAudioMidi::stopSynthSession(sessionId);
			if (driver)
				AIL_close_digital_driver(driver);
			AIL_shutdown();
			if (restoreConsoleMode)
				SetConsoleMode(consoleInput, originalConsoleMode);
		}

		bool initialise()
		{
			if (!AIL_startup())
			{
				std::cerr << "Could not initialise JUCE audio: " << AIL_last_error() << '\n';
				return false;
			}

			driver = AIL_open_digital_driver(48000, 16, MSS_MC_STEREO, 0);
			if (!driver)
			{
				std::cerr << "Could not open the default audio output: " << AIL_last_error() << '\n';
				return false;
			}

			ClientAudioMidi::startSynthSession(sessionId, instrumentId);
			refreshMidiDevices(false);
			DWORD mode = 0;
			if (consoleInput != INVALID_HANDLE_VALUE && GetConsoleMode(consoleInput, &mode))
			{
				originalConsoleMode = mode;
				restoreConsoleMode = true;
				mode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
				mode &= ~ENABLE_QUICK_EDIT_MODE;
				SetConsoleMode(consoleInput, mode);
				FlushConsoleInputBuffer(consoleInput);
			}

			S32 rate = 0;
			S32 format = 0;
			char description[256] = {};
			AIL_digital_configuration(driver, &rate, &format, description);
			std::cout << "Audio: " << description << " at " << rate << " Hz\n";
			std::cout << "Original-timbre sample bank: " << ClientAudioMidi::getLoadedSynthSampleCount() << "/14 instruments\n";
			printControls();
			printStatus();
			return true;
		}

		void run(std::string const &initialMidiFile)
		{
			SetConsoleCtrlHandler(consoleControlHandler, TRUE);
			if (!initialMidiFile.empty())
				loadAndStartScript(initialMidiFile);

			auto previous = Clock::now();
			while (!quitRequested)
			{
				auto const now = Clock::now();
				double const elapsed = std::chrono::duration<double>(now - previous).count();
				previous = now;

				pollKeyboard();
				pollMidiInput();
				advanceScript(elapsed);
				updateWindowTitle();
				std::this_thread::sleep_for(std::chrono::milliseconds(4));
			}
			SetConsoleCtrlHandler(consoleControlHandler, FALSE);
		}

	private:
		void printControls() const
		{
			std::cout
				<< "\nEntertainer Reborn local audio/input test (no network)\n"
				<< "--------------------------------------------------------\n"
				<< "Notes: Z S X D C V G B H N J M , L . ; / Q 2 W 3 E R 5\n"
				<< "[ / ]        Shift octave\n"
				<< "PageUp/Down  Previous/next entertainer instrument\n"
				<< "F2           Rescan and select next MIDI input\n"
				<< "F4           Open and play a Standard MIDI File\n"
				<< "F8           Pause/resume MIDI-file playback\n"
				<< "F9           Stop MIDI-file playback\n"
				<< "Space        All notes off\n"
				<< "Escape       Exit\n\n"
				<< "Keyboard notes are active only while this console is focused.\n";
		}

		void printStatus() const
		{
			std::cout << "Instrument " << instrumentId << "/14: "
				<< ClientAudioMidi::getSynthPatchName(instrumentId)
				<< " | octave " << (octaveShift >= 0 ? "+" : "") << octaveShift;
			if (midiDeviceIndex >= 0 && midiDeviceIndex < static_cast<int>(midiDevices.size()))
				std::cout << " | MIDI: " << midiDevices[static_cast<size_t>(midiDeviceIndex)].name;
			else
				std::cout << " | MIDI: keyboard only";
			std::cout << '\n';
		}

		void updateWindowTitle() const
		{
			std::string title = "Entertainer Audio Test | ";
			title += ClientAudioMidi::getSynthPatchName(instrumentId);
			if (scriptPlaying)
				title += " | " + std::string(scriptPaused ? "Paused " : "Playing ") + formatTime(scriptPosition) + "/" + formatTime(scriptDuration);
			SetConsoleTitleA(title.c_str());
		}

		void pollKeyboard()
		{
			DWORD eventCount = 0;
			if (consoleInput == INVALID_HANDLE_VALUE || !GetNumberOfConsoleInputEvents(consoleInput, &eventCount))
				return;
			while (eventCount-- > 0)
			{
				INPUT_RECORD input = {};
				DWORD read = 0;
				if (!ReadConsoleInputA(consoleInput, &input, 1, &read) || read != 1)
					break;
				if (input.EventType == FOCUS_EVENT && !input.Event.FocusEvent.bSetFocus)
				{
					releaseKeyboardNotes();
					keyState.fill(false);
					continue;
				}
				if (input.EventType != KEY_EVENT)
					continue;

				KEY_EVENT_RECORD const &key = input.Event.KeyEvent;
				int const virtualKey = key.wVirtualKeyCode;
				if (virtualKey < 0 || virtualKey >= static_cast<int>(keyState.size()))
					continue;
				bool const down = key.bKeyDown != FALSE;
				bool const firstPress = down && !keyState[static_cast<size_t>(virtualKey)];
				keyState[static_cast<size_t>(virtualKey)] = down;

				for (size_t index = 0; index < keyboardBindings.size(); ++index)
				{
					if (keyboardBindings[index].virtualKey != virtualKey)
						continue;
					if (down && keyboardNotes[index] < 0)
					{
						int const note = (std::max)(0, (std::min)(127, 48 + static_cast<int>(index) + octaveShift * 12));
						keyboardNotes[index] = note;
						ClientAudioMidi::synthNoteOn(sessionId, 0, note, 100);
					}
					else if (!down && keyboardNotes[index] >= 0)
					{
						ClientAudioMidi::synthNoteOff(sessionId, 0, keyboardNotes[index]);
						keyboardNotes[index] = -1;
					}
					break;
				}

				if (!firstPress)
					continue;
				if (virtualKey == VK_ESCAPE)
					quitRequested = true;
				else if (virtualKey == VK_SPACE)
					allNotesOff();
				else if (virtualKey == VK_OEM_4)
					setOctave(octaveShift - 1);
				else if (virtualKey == VK_OEM_6)
					setOctave(octaveShift + 1);
				else if (virtualKey == VK_PRIOR)
					setInstrument(instrumentId - 1);
				else if (virtualKey == VK_NEXT)
					setInstrument(instrumentId + 1);
				else if (virtualKey == VK_F2)
					refreshMidiDevices(true);
				else if (virtualKey == VK_F4)
				{
					allNotesOff();
					std::string const path = selectMidiFile(GetForegroundWindow());
					if (!path.empty())
						loadAndStartScript(path);
				}
				else if (virtualKey == VK_F8)
					toggleScriptPaused();
				else if (virtualKey == VK_F9)
					stopScript(true);
			}
		}

		void releaseKeyboardNotes()
		{
			for (int &note : keyboardNotes)
			{
				if (note >= 0)
					ClientAudioMidi::synthNoteOff(sessionId, 0, note);
				note = -1;
			}
		}

		void setInstrument(int requestedInstrument)
		{
			int const wrapped = requestedInstrument < 1 ? 14 : (requestedInstrument > 14 ? 1 : requestedInstrument);
			allNotesOff();
			ClientAudioMidi::stopSynthSession(sessionId);
			instrumentId = wrapped;
			ClientAudioMidi::startSynthSession(sessionId, instrumentId);
			printStatus();
		}

		void setOctave(int requestedOctave)
		{
			int const clamped = (std::max)(-3, (std::min)(3, requestedOctave));
			if (clamped == octaveShift)
				return;
			releaseKeyboardNotes();
			octaveShift = clamped;
			printStatus();
		}

		void refreshMidiDevices(bool selectNext)
		{
			std::string previousIdentifier;
			if (midiDeviceIndex >= 0 && midiDeviceIndex < static_cast<int>(midiDevices.size()))
				previousIdentifier = midiDevices[static_cast<size_t>(midiDeviceIndex)].identifier;

			ClientAudioMidi::closeInput();
			midiDevices = ClientAudioMidi::getInputDevices();
			if (midiDevices.empty())
			{
				midiDeviceIndex = -1;
				std::cout << "No MIDI input devices found; keyboard input remains available.\n";
				return;
			}

			int index = 0;
			if (!previousIdentifier.empty())
				for (size_t i = 0; i < midiDevices.size(); ++i)
					if (midiDevices[i].identifier == previousIdentifier)
						index = static_cast<int>(i);
			if (selectNext)
				index = (index + 1) % static_cast<int>(midiDevices.size());

			std::string error;
			if (ClientAudioMidi::openInput(midiDevices[static_cast<size_t>(index)].identifier, error))
			{
				midiDeviceIndex = index;
				std::cout << "MIDI input: " << midiDevices[static_cast<size_t>(index)].name << '\n';
			}
			else
			{
				midiDeviceIndex = -1;
				std::cout << "Could not open MIDI input: " << error << '\n';
			}
		}

		void pollMidiInput()
		{
			ClientAudioMidiEvent event = {};
			int budget = 256;
			while (budget-- > 0 && ClientAudioMidi::pollEvent(event))
				dispatchEvent(event, false);
		}

		void dispatchEvent(ClientAudioMidiEvent const &event, bool scriptEvent)
		{
			int const channel = (std::max)(0, (std::min)(15, event.channel - 1));
			int const sourceNote = (std::max)(0, (std::min)(127, event.note));
			if (event.type == ClientAudioMidiEvent::T_noteOn && event.value > 0)
			{
				int const shifted = (std::max)(0, (std::min)(127, sourceNote + octaveShift * 12));
				int const active = activeMidiNotes[static_cast<size_t>(channel)][static_cast<size_t>(sourceNote)];
				if (active >= 0 && !scriptEvent)
					ClientAudioMidi::synthNoteOff(sessionId, channel, active);
				activeMidiNotes[static_cast<size_t>(channel)][static_cast<size_t>(sourceNote)] = shifted;
				ClientAudioMidi::synthNoteOn(sessionId, channel, shifted, event.value);
			}
			else if (event.type == ClientAudioMidiEvent::T_noteOff || (event.type == ClientAudioMidiEvent::T_noteOn && event.value == 0))
			{
				int &active = activeMidiNotes[static_cast<size_t>(channel)][static_cast<size_t>(sourceNote)];
				if (active >= 0)
					ClientAudioMidi::synthNoteOff(sessionId, channel, active);
				active = -1;
			}
			else if (event.type == ClientAudioMidiEvent::T_controlChange && event.note == 64)
				ClientAudioMidi::synthSustain(sessionId, channel, event.value >= 64);
			else if (event.type == ClientAudioMidiEvent::T_controlChange && (event.note == 120 || event.note == 123))
				allNotesOff();
		}

		void loadAndStartScript(std::string const &path)
		{
			std::vector<ClientAudioMidiEvent> events;
			ClientAudioMidiSequenceInfo info = {};
			std::string error;
			if (!ClientAudioMidi::loadMidiSequence(path, events, info, error))
			{
				std::cout << "MIDI file rejected: " << error << '\n';
				return;
			}

			stopScript(false);
			scriptEvents.swap(events);
			scriptEventIndex = 0;
			scriptPosition = 0.0;
			scriptDuration = info.durationSeconds;
			scriptPath = path;
			scriptPlaying = true;
			scriptPaused = false;
			std::cout << "Playing MIDI file: " << path << " (format " << info.fileType
				<< ", " << info.trackCount << " tracks, " << info.eventCount << " events, "
				<< formatTime(info.durationSeconds) << ")\n";
		}

		void advanceScript(double elapsed)
		{
			if (!scriptPlaying || scriptPaused)
				return;
			scriptPosition += (std::max)(0.0, elapsed);
			int budget = 256;
			while (budget-- > 0 && scriptEventIndex < scriptEvents.size() && scriptEvents[scriptEventIndex].timestampSeconds <= scriptPosition)
				dispatchEvent(scriptEvents[scriptEventIndex++], true);
			if (scriptEventIndex >= scriptEvents.size() && scriptPosition >= scriptDuration + 0.05)
			{
				std::cout << "Completed MIDI file: " << scriptPath << '\n';
				stopScript(false);
			}
		}

		void toggleScriptPaused()
		{
			if (!scriptPlaying)
			{
				std::cout << "No MIDI file is playing. Press F4 to choose one.\n";
				return;
			}
			scriptPaused = !scriptPaused;
			if (scriptPaused)
				allNotesOff();
			std::cout << (scriptPaused ? "MIDI-file playback paused.\n" : "MIDI-file playback resumed.\n");
		}

		void stopScript(bool report)
		{
			if (scriptPlaying)
				allNotesOff();
			scriptEvents.clear();
			scriptEventIndex = 0;
			scriptPosition = 0.0;
			scriptDuration = 0.0;
			scriptPlaying = false;
			scriptPaused = false;
			if (report)
				std::cout << "MIDI-file playback stopped.\n";
		}

		void allNotesOff()
		{
			ClientAudioMidi::synthAllNotesOff(sessionId);
			keyboardNotes.fill(-1);
			for (std::array<int, 128> &channel : activeMidiNotes)
				channel.fill(-1);
		}

		HDIGDRIVER driver;
		int instrumentId;
		int octaveShift;
		std::array<bool, 256> keyState = {};
		std::array<int, 24> keyboardNotes;
		std::array<std::array<int, 128>, 16> activeMidiNotes;
		std::vector<ClientAudioMidiDevice> midiDevices;
		int midiDeviceIndex;
		std::vector<ClientAudioMidiEvent> scriptEvents;
		size_t scriptEventIndex;
		double scriptPosition;
		double scriptDuration;
		bool scriptPlaying;
		bool scriptPaused;
		std::string scriptPath;
		HANDLE consoleInput;
		DWORD originalConsoleMode;
		bool restoreConsoleMode;
	};
}

int main(int argc, char **argv)
{
	Harness harness;
	if (!harness.initialise())
	{
		std::cerr << "\nPress Enter to close.\n";
		std::cin.get();
		return 1;
	}

	std::string initialMidiFile;
	if (argc > 1)
		initialMidiFile = argv[1];
	harness.run(initialMidiFile);
	return 0;
}
