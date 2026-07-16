#ifndef INCLUDED_ClientAudioMidi_H
#define INCLUDED_ClientAudioMidi_H

#include <string>
#include <vector>

struct ClientAudioMidiDevice
{
	std::string name;
	std::string identifier;
};

struct ClientAudioMidiEvent
{
	enum Type
	{
		T_noteOn,
		T_noteOff,
		T_controlChange
	};

	Type type;
	int channel;
	int note;
	int value;
	double timestampSeconds;
};

class ClientAudioMidi
{
public:
	static std::vector<ClientAudioMidiDevice> getInputDevices();
	static bool openInput(std::string const &identifier, std::string &errorMessage);
	static bool openFirstAvailableInput(std::string &deviceName, std::string &errorMessage);
	static void closeInput();
	static bool isInputOpen();
	static std::string getOpenInputIdentifier();
	static bool pollEvent(ClientAudioMidiEvent &event);
	static void clearEvents();
};

#endif
