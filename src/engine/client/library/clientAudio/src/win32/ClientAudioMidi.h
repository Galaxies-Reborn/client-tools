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

	static void startSynthSession(unsigned long long performerId, int instrumentId);
	static void stopSynthSession(unsigned long long performerId);
	static void synthNoteOn(unsigned long long performerId, int channel, int note, int velocity);
	static void synthNoteOff(unsigned long long performerId, int channel, int note);
	static void synthSustain(unsigned long long performerId, int channel, bool enabled);
	static void synthAllNotesOff(unsigned long long performerId);
	static char const *getSynthPatchName(int instrumentId);
};

#endif
