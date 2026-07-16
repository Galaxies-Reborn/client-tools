#ifndef INCLUDED_PerformanceMidiMessage_H
#define INCLUDED_PerformanceMidiMessage_H

namespace PerformanceMidiMessage
{
	enum Type
	{
		T_invalid = 0,
		T_sessionStart = 1,
		T_noteOn = 2,
		T_noteOff = 3,
		T_allNotesOff = 4,
		T_sustain = 5,
		T_sessionStop = 6
	};

	struct Decoded
	{
		Type type;
		int channel;
		int note;
		int value;
	};

	int const cms_signature = 0x40000000;
	int const cms_signatureMask = 0x7fe00000;

	inline int encode(Type type, int channel, int note, int value)
	{
		return cms_signature |
			((static_cast<int>(type) & 0x7) << 18) |
			((channel & 0xf) << 14) |
			((note & 0x7f) << 7) |
			(value & 0x7f);
	}

	inline bool decode(int encoded, Decoded &decoded)
	{
		if ((encoded & cms_signatureMask) != cms_signature)
			return false;
		decoded.type = static_cast<Type>((encoded >> 18) & 0x7);
		decoded.channel = (encoded >> 14) & 0xf;
		decoded.note = (encoded >> 7) & 0x7f;
		decoded.value = encoded & 0x7f;
		return decoded.type >= T_sessionStart && decoded.type <= T_sessionStop;
	}
}

#endif
