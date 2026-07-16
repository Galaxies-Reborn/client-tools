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
	static void stopPerformanceMode(bool sendServerStop = true);
	static bool triggerFlourish(int flourishNumber);

	static Mode getMode();
	static bool isActive();
	static std::string const &getSongName();
	static std::string const &getMidiDeviceName();
	static std::vector<MidiDevice> getMidiDevices();
	static bool selectMidiDevice(std::string const &identifier, std::string &statusMessage);

private:
	static void remove();

	PerformanceModeManager();
	PerformanceModeManager(PerformanceModeManager const &);
	PerformanceModeManager &operator=(PerformanceModeManager const &);
};

#endif
