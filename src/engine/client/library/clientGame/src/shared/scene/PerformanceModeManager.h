#ifndef INCLUDED_PerformanceModeManager_H
#define INCLUDED_PerformanceModeManager_H

#include <string>
#include <vector>

struct IoEvent;

class PerformanceModeManager
{
public:
	enum Mode
	{
		M_none,
		M_midiToFlourish,
		M_midiToMusic,
		M_musicFromScript
	};

	struct MidiDevice
	{
		std::string name;
		std::string identifier;
	};

	static void install();
	static void alter(float elapsedTime);
	static bool processEvent(IoEvent const &event);

	static bool startMidiToFlourish(std::string const &songName, std::string &statusMessage);
	static bool startMidiToMusic(std::string &statusMessage);
	static bool startMusicFromScript(std::string const &fileName, std::string &statusMessage);
	static void stopPerformanceMode(bool sendServerStop = true);
	static bool triggerFlourish(int flourishNumber);
	static bool toggleScriptPaused();

	static Mode getMode();
	static Mode getRequestedMode();
	static bool isActive();
	static bool isPending();
	static bool hasPerformanceMode();
	static std::string const &getSongName();
	static std::string const &getStatusMessage();
	static std::vector<std::string> getAvailableMusicSongs();
	static std::vector<std::string> getAvailableMidiScripts();
	static std::string getMidiDirectory();
	static std::string const &getMidiDeviceName();
	static std::string const &getInstrumentName();
	static std::string const &getScriptFileName();
	static double getScriptPositionSeconds();
	static double getScriptDurationSeconds();
	static bool isScriptPaused();
	static std::vector<MidiDevice> getMidiDevices();
	static bool selectMidiDevice(std::string const &identifier, std::string &statusMessage);

	static int getFlourishKey(int flourishIndex);
	static void setFlourishKey(int flourishIndex, int scanCode);
	static int getDefaultFlourishKey(int flourishIndex);
	static int getFlourishMidiNote(int flourishIndex);
	static void setFlourishMidiNote(int flourishIndex, int midiNote);
	static int getDefaultFlourishMidiNote(int flourishIndex);
	static int getMusicKey(int noteIndex);
	static void setMusicKey(int noteIndex, int scanCode);
	static int getDefaultMusicKey(int noteIndex);
	static int getMidiOctaveShift();
	static void setMidiOctaveShift(int octaveShift);
	static std::string const &getSelectedMidiDeviceIdentifier();
	static void resetInputMappings();

private:
	static void remove();

	PerformanceModeManager();
	PerformanceModeManager(PerformanceModeManager const &);
	PerformanceModeManager &operator=(PerformanceModeManager const &);
};

#endif
