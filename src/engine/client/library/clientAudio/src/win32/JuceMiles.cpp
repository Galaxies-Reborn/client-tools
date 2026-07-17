#include "clientAudio/FirstClientAudio.h"
#include "clientAudio/ClientAudioMidi.h"

#if defined(_WIN64)

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#pragma push_macro("U8")
#pragma push_macro("U16")
#undef U8
#undef U16
#include "../../../../../../external/3rd/library/JUCE-8.0.14/modules/juce_audio_devices/juce_audio_devices.h"
#include "../../../../../../external/3rd/library/JUCE-8.0.14/modules/juce_audio_formats/juce_audio_formats.h"
#pragma pop_macro("U16")
#pragma pop_macro("U8")

namespace JuceMilesNamespace
{
using FrameIndex = std::uint64_t;

struct Vector3
{
	F32 x = 0.0f;
	F32 y = 0.0f;
	F32 z = 0.0f;
};

struct SampleState;

struct DriverState final : public juce::AudioIODeviceCallback
{
	HDIGDRIVER handle = nullptr;
	juce::AudioDeviceManager deviceManager;
	juce::Reverb reverb;
	juce::AudioBuffer<float> wetBuffer;
	double sampleRate = 44100.0;
	S32 outputChannels = 2;
	S32 bufferSize = 0;
	MSS_MC_SPEC channelSpec = MSS_MC_STEREO;
	S32 roomType = ENVIRONMENT_GENERIC;
	F32 rolloffFactor = 1.0f;
	F32 masterVolume = 1.0f;
	F32 masterDryLevel = 1.0f;
	F32 masterWetLevel = 1.0f;
	Vector3 listenerPosition;
	Vector3 listenerVelocity;
	Vector3 listenerFront = {0.0f, 0.0f, -1.0f};
	Vector3 listenerUp = {0.0f, 1.0f, 0.0f};
	std::atomic<bool> deviceRunning = false;

	void audioDeviceIOCallbackWithContext(
		float const *const *,
		int,
		float *const *outputChannelData,
		int numOutputChannels,
		int numSamples,
		juce::AudioIODeviceCallbackContext const &) override;
	void audioDeviceAboutToStart(juce::AudioIODevice *device) override;
	void audioDeviceStopped() override;
	void audioDeviceError(juce::String const &message) override;
};

struct SampleState
{
	explicit SampleState(HDIGDRIVER driverHandle) : driver(driverHandle) {}

	HDIGDRIVER driver = nullptr;
	juce::AudioBuffer<float> pcm;
	std::vector<U8> encodedData;
	FrameIndex totalFrames = 0;
	FrameIndex seekFrame = 0;
	double cursorFrame = 0.0;
	S32 sourceChannels = 0;
	S32 sourceSampleRate = 0;
	S32 sourceBits = 0;
	S32 encodedBlockAlign = 1;
	S32 playbackRate = 0;
	F32 leftVolume = 1.0f;
	F32 rightVolume = 1.0f;
	F32 dryLevel = 1.0f;
	F32 wetLevel = 0.0f;
	F32 obstruction = 0.0f;
	F32 occlusion = 0.0f;
	F32 positionX = 0.0f;
	F32 positionY = 0.0f;
	F32 positionZ = 0.0f;
	F32 velocityX = 0.0f;
	F32 velocityY = 0.0f;
	F32 velocityZ = 0.0f;
	F32 minDistance = 1.0f;
	F32 maxDistance = 100.0f;
	F32 filterLeft = 0.0f;
	F32 filterRight = 0.0f;
	S32 loopCount = 1;
	S32 remainingLoops = 1;
	S32 loopStartOffset = 0;
	S32 loopEndOffset = -1;
	bool loopActive = false;
	AILSAMPLECB eosCallback = nullptr;
	HSTREAM ownerStream = nullptr;
	std::atomic<bool> completed = false;
	U32 status = SMP_DONE;
	bool spatialized = false;
};

struct StreamState
{
	HSAMPLE sample = nullptr;
	AILSTREAMCB callback = nullptr;
	std::string filename;
};

using DriverMap = std::unordered_map<HDIGDRIVER, std::unique_ptr<DriverState>>;
using SampleMap = std::unordered_map<HSAMPLE, std::unique_ptr<SampleState>>;
using StreamMap = std::unordered_map<HSTREAM, std::unique_ptr<StreamState>>;

std::recursive_mutex s_mutex;
DriverMap s_drivers;
SampleMap s_samples;
StreamMap s_streams;
AIL_file_open_callback s_fileOpenCallback = nullptr;
AIL_file_close_callback s_fileCloseCallback = nullptr;
AIL_file_seek_callback s_fileSeekCallback = nullptr;
AIL_file_read_callback s_fileReadCallback = nullptr;
std::unordered_map<U32, SINTa> s_preferences;
std::unique_ptr<juce::ScopedJuceInitialiser_GUI> s_juceInitialiser;
juce::AudioFormatManager s_formatManager;
std::string s_redistDirectory;
std::string s_lastError;
S32 s_fileError = AIL_NO_ERROR;
bool s_started = false;

class MidiInputCallback final : public juce::MidiInputCallback
{
public:
	void handleIncomingMidiMessage(juce::MidiInput *, juce::MidiMessage const &message) override;
};

std::mutex s_midiMutex;
std::deque<ClientAudioMidiEvent> s_midiEvents;
std::unique_ptr<juce::MidiInput> s_midiInput;
std::string s_midiInputIdentifier;
MidiInputCallback s_midiInputCallback;
size_t const s_maximumQueuedMidiEvents = 2048;

enum SynthEnvelopeStage
{
	SES_attack,
	SES_decay,
	SES_sustain,
	SES_release
};

enum SynthToneModel
{
	STM_strings,
	STM_reed,
	STM_brass,
	STM_flute,
	STM_horn,
	STM_pluck,
	STM_percussion,
	STM_metallicBox,
	STM_organ,
	STM_mandoviol,
	STM_bells,
	STM_flanged,
	STM_valahorn,
	STM_downeyBox
};

struct SynthPatch
{
	char const *name;
	SynthToneModel toneModel;
	float attackSeconds;
	float decaySeconds;
	float sustainLevel;
	float releaseSeconds;
	float detune;
	float gain;
	float filterCoefficient;
	float noiseAmount;
	float modulationRatio;
	float modulationDepth;
	bool oneShot;
};

SynthPatch const s_synthPatches[14] =
{
	{"Traz Strings",        STM_strings,      0.025f, 0.34f, 0.68f, 0.55f,  0.0035f, 0.105f, 0.20f, 0.010f, 1.000f, 0.00f, false},
	{"Slitherhorn Reed",    STM_reed,         0.032f, 0.18f, 0.80f, 0.26f, -0.0010f, 0.082f, 0.42f, 0.035f, 1.000f, 0.00f, false},
	{"Fanfar Brass",        STM_brass,        0.018f, 0.16f, 0.76f, 0.34f,  0.0008f, 0.078f, 0.55f, 0.000f, 1.000f, 0.00f, false},
	{"Droopy Flute",        STM_flute,        0.065f, 0.20f, 0.88f, 0.32f,  0.0015f, 0.145f, 0.16f, 0.045f, 1.000f, 0.00f, false},
	{"Kloo Horn",           STM_horn,         0.028f, 0.22f, 0.74f, 0.30f, -0.0020f, 0.105f, 0.28f, 0.008f, 1.000f, 0.00f, false},
	{"Fizz Pluck",          STM_pluck,        0.002f, 0.82f, 0.00f, 0.10f,  0.0060f, 0.180f, 0.58f, 0.080f, 2.010f, 0.60f, true },
	{"Bandfill Percussion", STM_percussion,   0.001f, 0.42f, 0.00f, 0.08f, -0.0040f, 0.155f, 0.72f, 0.820f, 1.370f, 0.00f, true },
	{"Omni Box",            STM_metallicBox,  0.003f, 1.05f, 0.00f, 0.14f,  0.0040f, 0.150f, 0.64f, 0.025f, 1.414f, 0.00f, true },
	{"Nalargon Organ",      STM_organ,        0.040f, 0.18f, 0.92f, 0.62f,  0.0000f, 0.070f, 0.48f, 0.000f, 2.000f, 0.00f, false},
	{"Mandoviol",           STM_mandoviol,    0.004f, 1.45f, 0.00f, 0.18f, -0.0035f, 0.155f, 0.46f, 0.025f, 2.000f, 0.00f, true },
	{"Xantha Bells",        STM_bells,        0.002f, 2.80f, 0.00f, 0.70f,  0.0080f, 0.135f, 0.88f, 0.000f, 2.730f, 5.20f, true },
	{"Flanged Jessoon",     STM_flanged,      0.030f, 0.24f, 0.78f, 0.48f,  0.0070f, 0.115f, 0.36f, 0.015f, 1.000f, 0.00f, false},
	{"Valahorn",            STM_valahorn,     0.052f, 0.26f, 0.82f, 0.46f, -0.0012f, 0.086f, 0.18f, 0.004f, 0.500f, 0.00f, false},
	{"Downey Box",          STM_downeyBox,    0.002f, 0.68f, 0.00f, 0.10f,  0.0090f, 0.165f, 0.50f, 0.180f, 1.510f, 0.00f, true }
};

struct SynthVoice
{
	bool active = false;
	bool heldBySustain = false;
	int channel = 0;
	int note = 0;
	float velocity = 0.0f;
	double phase = 0.0;
	double detunedPhase = 0.0;
	double modulationPhase = 0.0;
	double sampleCursor = 0.0;
	float envelope = 0.0f;
	float filterState = 0.0f;
	std::uint32_t noiseState = 0x6d2b79f5u;
	SynthEnvelopeStage stage = SES_attack;
	std::uint64_t age = 0;
};

struct SynthSample
{
	juce::AudioBuffer<float> pcm;
	double sampleRate = 48000.0;
	bool loaded = false;
};

struct SynthSession
{
	int instrumentId = 1;
	std::array<bool, 16> sustain = {};
	std::array<SynthVoice, 64> voices;
	std::uint64_t nextVoiceAge = 1;
};

using SynthSessionMap = std::unordered_map<unsigned long long, SynthSession>;
SynthSessionMap s_synthSessions;
std::array<SynthSample, 14> s_synthSamples;
int s_loadedSynthSampleCount = 0;

SynthPatch const &getSynthPatch(int instrumentId)
{
	int const index = (std::max)(1, (std::min)(14, instrumentId)) - 1;
	return s_synthPatches[index];
}

SynthSample const *getSynthSample(int instrumentId)
{
	int const index = (std::max)(1, (std::min)(14, instrumentId)) - 1;
	SynthSample const &sample = s_synthSamples[static_cast<size_t>(index)];
	return sample.loaded ? &sample : nullptr;
}

int loadSynthSampleBank(juce::File const &directory)
{
	for (SynthSample &sample : s_synthSamples)
		sample = SynthSample();
	s_loadedSynthSampleCount = 0;
	if (!directory.isDirectory())
		return 0;

	for (int instrumentId = 1; instrumentId <= 14; ++instrumentId)
	{
		juce::String const filename = juce::String::formatted("instrument_%02d.wav", instrumentId);
		juce::File const file = directory.getChildFile(filename);
		std::unique_ptr<juce::AudioFormatReader> reader(s_formatManager.createReaderFor(file));
		if (!reader || reader->lengthInSamples < 2 || reader->lengthInSamples > static_cast<juce::int64>((std::numeric_limits<int>::max)()))
			continue;

		SynthSample &sample = s_synthSamples[static_cast<size_t>(instrumentId - 1)];
		int const channels = (std::max)(1, (std::min)(2, static_cast<int>(reader->numChannels)));
		sample.pcm.setSize(channels, static_cast<int>(reader->lengthInSamples));
		if (!reader->read(&sample.pcm, 0, sample.pcm.getNumSamples(), 0, true, true))
		{
			sample = SynthSample();
			continue;
		}
		sample.sampleRate = reader->sampleRate;
		sample.loaded = true;
		++s_loadedSynthSampleCount;
	}
	return s_loadedSynthSampleCount;
}

void discoverSynthSampleBank()
{
	juce::File const executableDirectory = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
	juce::File const executableBank = executableDirectory.getChildFile("midi").getChildFile("instruments");
	if (loadSynthSampleBank(executableBank) > 0)
		return;

	juce::File const workingBank = juce::File::getCurrentWorkingDirectory().getChildFile("midi").getChildFile("instruments");
	if (workingBank != executableBank)
		loadSynthSampleBank(workingBank);
}

void releaseSynthVoice(SynthVoice &voice)
{
	if (voice.active)
	{
		voice.heldBySustain = false;
		voice.stage = SES_release;
	}
}

float nextSynthNoise(SynthVoice &voice)
{
	voice.noiseState ^= voice.noiseState << 13;
	voice.noiseState ^= voice.noiseState >> 17;
	voice.noiseState ^= voice.noiseState << 5;
	return static_cast<float>(voice.noiseState & 0xffffu) / 32767.5f - 1.0f;
}

float synthTriangle(double phase)
{
	return static_cast<float>((2.0 / 3.14159265358979323846) * std::asin(std::sin(phase)));
}

float synthSaw(double phase)
{
	return static_cast<float>(0.56 * (std::sin(phase) + 0.50 * std::sin(phase * 2.0) + 0.33 * std::sin(phase * 3.0) + 0.25 * std::sin(phase * 4.0)));
}

float synthSquare(double phase)
{
	return static_cast<float>(0.72 * (std::sin(phase) + 0.33 * std::sin(phase * 3.0) + 0.20 * std::sin(phase * 5.0)));
}

void renderSynthSessions(DriverState const &driver, float *const *outputs, int outputChannels, int numSamples)
{
	if (s_synthSessions.empty() || !outputs[0])
		return;

	constexpr double twoPi = 6.28318530717958647692;
	double const sampleRate = (std::max)(driver.sampleRate, 1.0);
	for (SynthSessionMap::value_type &entry : s_synthSessions)
	{
		SynthSession &session = entry.second;
		SynthPatch const &patch = getSynthPatch(session.instrumentId);
		SynthSample const *const instrumentSample = getSynthSample(session.instrumentId);
		for (SynthVoice &voice : session.voices)
		{
			if (!voice.active)
				continue;

			double const frequency = 440.0 * std::pow(2.0, static_cast<double>(voice.note - 69) / 12.0);
			double const phaseStep = twoPi * frequency / sampleRate;
			double const detunedStep = phaseStep * (1.0 + patch.detune);
			double const sampleStep = instrumentSample
				? (instrumentSample->sampleRate / sampleRate) * std::pow(2.0, static_cast<double>(voice.note - 60) / 12.0)
				: 0.0;
			float const attackStep = 1.0f / static_cast<float>(sampleRate * (std::max)(patch.attackSeconds, 0.001f));
			float const decayStep = (1.0f - patch.sustainLevel) / static_cast<float>(sampleRate * (std::max)(patch.decaySeconds, 0.001f));
			float const releaseStep = 1.0f / static_cast<float>(sampleRate * (std::max)(patch.releaseSeconds, 0.001f));

			for (int frame = 0; frame < numSamples; ++frame)
			{
				switch (voice.stage)
				{
				case SES_attack:
					voice.envelope += attackStep;
					if (voice.envelope >= 1.0f)
					{
						voice.envelope = 1.0f;
						voice.stage = SES_decay;
					}
					break;
				case SES_decay:
					if (patch.oneShot)
					{
						voice.envelope -= 1.0f / static_cast<float>(sampleRate * (std::max)(patch.decaySeconds, 0.001f));
						if (voice.envelope <= 0.0f)
						{
							voice.envelope = 0.0f;
							voice.active = false;
						}
					}
					else
					{
						voice.envelope -= decayStep;
						if (voice.envelope <= patch.sustainLevel)
						{
							voice.envelope = patch.sustainLevel;
							voice.stage = SES_sustain;
						}
					}
					break;
				case SES_release:
					voice.envelope -= releaseStep;
					if (voice.envelope <= 0.0f)
					{
						voice.envelope = 0.0f;
						voice.active = false;
					}
					break;
				case SES_sustain:
				default:
					break;
				}

				if (!voice.active)
					break;

				double const phase = voice.phase;
				double const detunedPhase = voice.detunedPhase;
				float const noise = nextSynthNoise(voice);
				float const sine = static_cast<float>(std::sin(phase));
				float sample = 0.0f;
				if (instrumentSample)
				{
					int const frameCount = instrumentSample->pcm.getNumSamples();
					while (voice.sampleCursor >= frameCount)
						voice.sampleCursor -= frameCount;
					int const firstFrame = static_cast<int>(voice.sampleCursor);
					int const secondFrame = firstFrame + 1 < frameCount ? firstFrame + 1 : 0;
					float const fraction = static_cast<float>(voice.sampleCursor - firstFrame);
					for (int channelIndex = 0; channelIndex < instrumentSample->pcm.getNumChannels(); ++channelIndex)
					{
						float const first = instrumentSample->pcm.getSample(channelIndex, firstFrame);
						float const second = instrumentSample->pcm.getSample(channelIndex, secondFrame);
						sample += first + fraction * (second - first);
					}
					sample /= static_cast<float>(instrumentSample->pcm.getNumChannels());
					voice.sampleCursor += sampleStep;
				}
				else switch (patch.toneModel)
				{
				case STM_strings:
					sample = 0.62f * synthSaw(phase) + 0.38f * synthSaw(detunedPhase) + patch.noiseAmount * noise;
					break;
				case STM_reed:
					sample = 0.72f * synthSquare(phase) + 0.18f * sine + patch.noiseAmount * noise;
					break;
				case STM_brass:
					sample = std::tanh((1.25f + voice.envelope) * (0.78f * synthSaw(phase) + 0.32f * sine));
					break;
				case STM_flute:
					sample = 0.94f * sine + 0.08f * static_cast<float>(std::sin(phase * 2.0)) + patch.noiseAmount * noise;
					break;
				case STM_horn:
					sample = 0.74f * synthTriangle(phase) + 0.26f * static_cast<float>(std::sin(phase * 2.0)) + patch.noiseAmount * noise;
					break;
				case STM_pluck:
					sample = 0.58f * synthTriangle(phase) + 0.32f * synthSaw(detunedPhase) + patch.noiseAmount * noise;
					break;
				case STM_percussion:
					sample = patch.noiseAmount * noise + 0.42f * static_cast<float>(std::sin(phase * (1.0 + 2.8 * voice.envelope)));
					break;
				case STM_metallicBox:
					sample = 0.88f * sine * static_cast<float>(std::sin(voice.modulationPhase)) + 0.22f * synthSquare(phase) + patch.noiseAmount * noise;
					break;
				case STM_organ:
					sample = static_cast<float>(0.62 * std::sin(phase) + 0.30 * std::sin(phase * 2.0) + 0.20 * std::sin(phase * 3.0) + 0.13 * std::sin(phase * 4.0) + 0.09 * std::sin(phase * 6.0));
					break;
				case STM_mandoviol:
					sample = 0.72f * synthSaw(phase) + 0.25f * synthSaw(detunedPhase) + patch.noiseAmount * noise;
					break;
				case STM_bells:
					sample = static_cast<float>(std::sin(phase + patch.modulationDepth * voice.envelope * std::sin(voice.modulationPhase)) + 0.18 * std::sin(phase * 3.01));
					break;
				case STM_flanged:
					sample = 0.72f * synthSaw(phase) - 0.52f * synthSaw(detunedPhase) + 0.28f * sine + patch.noiseAmount * noise;
					break;
				case STM_valahorn:
					sample = std::tanh(1.35f * static_cast<float>(std::sin(phase) + 0.52 * std::sin(phase * 2.0) + 0.22 * std::sin(phase * 3.0)));
					break;
				case STM_downeyBox:
					sample = 0.58f * synthSquare(phase) + 0.30f * sine * static_cast<float>(std::sin(voice.modulationPhase)) + patch.noiseAmount * noise;
					break;
				}

				if (instrumentSample)
					sample *= voice.envelope * voice.velocity * 0.34f * driver.masterVolume;
				else
				{
					float filterCoefficient = patch.filterCoefficient;
					if (patch.oneShot && patch.toneModel != STM_bells)
						filterCoefficient *= 0.18f + 0.82f * voice.envelope;
					voice.filterState += filterCoefficient * (sample - voice.filterState);
					sample = voice.filterState;
					sample *= voice.envelope * voice.velocity * patch.gain * driver.masterVolume;
				}
				outputs[0][frame] += sample;
				if (outputChannels > 1 && outputs[1])
					outputs[1][frame] += sample;

				voice.phase += phaseStep;
				voice.detunedPhase += detunedStep;
				voice.modulationPhase += phaseStep * patch.modulationRatio;
				if (voice.phase >= twoPi)
					voice.phase = std::fmod(voice.phase, twoPi);
				if (voice.detunedPhase >= twoPi)
					voice.detunedPhase = std::fmod(voice.detunedPhase, twoPi);
				if (voice.modulationPhase >= twoPi)
					voice.modulationPhase = std::fmod(voice.modulationPhase, twoPi);
			}
		}
	}
}

template <typename T>
T clampValue(T value, T low, T high)
{
	return value < low ? low : (value > high ? high : value);
}

void MidiInputCallback::handleIncomingMidiMessage(juce::MidiInput *, juce::MidiMessage const &message)
{
	ClientAudioMidiEvent event = {};
	if (message.isNoteOn())
	{
		event.type = ClientAudioMidiEvent::T_noteOn;
		event.note = message.getNoteNumber();
		event.value = message.getVelocity();
	}
	else if (message.isNoteOff())
	{
		event.type = ClientAudioMidiEvent::T_noteOff;
		event.note = message.getNoteNumber();
		event.value = message.getVelocity();
	}
	else if (message.isController())
	{
		event.type = ClientAudioMidiEvent::T_controlChange;
		event.note = message.getControllerNumber();
		event.value = message.getControllerValue();
	}
	else
	{
		return;
	}

	event.channel = message.getChannel();
	event.timestampSeconds = message.getTimeStamp();

	std::lock_guard<std::mutex> lock(s_midiMutex);
	if (s_midiEvents.size() >= s_maximumQueuedMidiEvents)
		s_midiEvents.pop_front();
	s_midiEvents.push_back(event);
}

U16 readU16(U8 const *data)
{
	return static_cast<U16>(data[0] | (static_cast<U16>(data[1]) << 8));
}

U32 readU32(U8 const *data)
{
	return static_cast<U32>(data[0]) |
		(static_cast<U32>(data[1]) << 8) |
		(static_cast<U32>(data[2]) << 16) |
		(static_cast<U32>(data[3]) << 24);
}

void setError(char const *message)
{
	s_lastError = message ? message : "Unknown JUCE audio error";
}

DriverState *findDriver(HDIGDRIVER driver)
{
	auto const iter = s_drivers.find(driver);
	return iter == s_drivers.end() ? nullptr : iter->second.get();
}

SampleState *findSample(HSAMPLE sample)
{
	auto const iter = s_samples.find(sample);
	return iter == s_samples.end() ? nullptr : iter->second.get();
}

StreamState *findStream(HSTREAM stream)
{
	auto const iter = s_streams.find(stream);
	return iter == s_streams.end() ? nullptr : iter->second.get();
}

Vector3 subtract(Vector3 const &a, Vector3 const &b)
{
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

F32 dot(Vector3 const &a, Vector3 const &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 cross(Vector3 const &a, Vector3 const &b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x};
}

F32 length(Vector3 const &value)
{
	return std::sqrt(dot(value, value));
}

Vector3 normalize(Vector3 value, Vector3 const &fallback)
{
	F32 const magnitude = length(value);
	if (magnitude <= 0.0001f)
		return fallback;
	F32 const inverse = 1.0f / magnitude;
	return {value.x * inverse, value.y * inverse, value.z * inverse};
}

bool isWave(U8 const *data, size_t size)
{
	return size >= 12 && std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WAVE", 4) == 0;
}

bool isOgg(U8 const *data, size_t size)
{
	return size >= 4 && std::memcmp(data, "OggS", 4) == 0;
}

bool isMp3(U8 const *data, size_t size)
{
	if (size >= 3 && std::memcmp(data, "ID3", 3) == 0)
		return true;
	for (size_t i = 0; i + 1 < (std::min)(size, static_cast<size_t>(4096)); ++i)
	{
		if (data[i] == 0xff && (data[i + 1] & 0xe0) == 0xe0)
			return true;
	}
	return false;
}

bool findWaveChunk(U8 const *data, size_t size, char const id[4], size_t &offset, U32 &chunkLength)
{
	if (!isWave(data, size))
		return false;

	size_t position = 12;
	while (position + 8 <= size)
	{
		U32 const declaredLength = readU32(data + position + 4);
		if (std::memcmp(data + position, id, 4) == 0)
		{
			offset = position + 8;
			chunkLength = static_cast<U32>((std::min)(static_cast<size_t>(declaredLength), size - offset));
			return true;
		}
		position += 8 + declaredLength + (declaredLength & 1u);
	}
	return false;
}

bool decodeAudio(SampleState &sample)
{
	if (sample.encodedData.empty())
	{
		setError("Audio data is empty");
		return false;
	}

	auto stream = std::make_unique<juce::MemoryInputStream>(sample.encodedData.data(), sample.encodedData.size(), false);
	std::unique_ptr<juce::AudioFormatReader> reader(s_formatManager.createReaderFor(std::move(stream)));
	if (!reader || reader->numChannels == 0 || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
	{
		setError("JUCE could not recognize or decode the audio data");
		return false;
	}
	if (reader->lengthInSamples > static_cast<juce::int64>((std::numeric_limits<int>::max)()))
	{
		setError("Audio asset is too large for the in-memory JUCE decoder");
		return false;
	}

	sample.sourceChannels = static_cast<S32>((std::min)(reader->numChannels, 2u));
	sample.sourceSampleRate = static_cast<S32>(reader->sampleRate + 0.5);
	sample.sourceBits = static_cast<S32>(reader->bitsPerSample);
	sample.totalFrames = static_cast<FrameIndex>(reader->lengthInSamples);
	sample.playbackRate = sample.sourceSampleRate;
	sample.encodedBlockAlign = (std::max)(1, sample.sourceChannels * (std::max)(1, sample.sourceBits / 8));

	if (isWave(sample.encodedData.data(), sample.encodedData.size()))
	{
		size_t formatOffset = 0;
		U32 formatLength = 0;
		if (findWaveChunk(sample.encodedData.data(), sample.encodedData.size(), "fmt ", formatOffset, formatLength) && formatLength >= 16)
			sample.encodedBlockAlign = (std::max)(1, static_cast<S32>(readU16(sample.encodedData.data() + formatOffset + 12)));
	}

	sample.pcm.setSize(sample.sourceChannels, static_cast<int>(sample.totalFrames), false, true, false);
	if (!reader->read(&sample.pcm, 0, static_cast<int>(sample.totalFrames), 0, true, sample.sourceChannels > 1))
	{
		sample.pcm.setSize(0, 0);
		setError("JUCE failed while decoding the audio data");
		return false;
	}

	sample.seekFrame = 0;
	sample.cursorFrame = 0.0;
	sample.completed.store(false, std::memory_order_release);
	sample.status = SMP_DONE;
	return true;
}

FrameIndex mapMp3OffsetToFrame(std::vector<U8> const &data, S32 requestedOffset)
{
	if (requestedOffset <= 0)
		return 0;

	size_t position = 0;
	if (data.size() >= 10 && std::memcmp(data.data(), "ID3", 3) == 0)
		position = 10 + ((data[6] & 0x7f) << 21) + ((data[7] & 0x7f) << 14) + ((data[8] & 0x7f) << 7) + (data[9] & 0x7f);

	static int const bitrateMpeg1Layer3[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
	static int const bitrateMpeg2Layer3[16] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
	static int const sampleRates[3] = {44100, 48000, 32000};
	FrameIndex frameCursor = 0;

	while (position + 4 <= data.size())
	{
		if (static_cast<S32>(position) >= requestedOffset)
			return frameCursor;
		U32 const header = (static_cast<U32>(data[position]) << 24) |
			(static_cast<U32>(data[position + 1]) << 16) |
			(static_cast<U32>(data[position + 2]) << 8) |
			static_cast<U32>(data[position + 3]);
		if ((header & 0xffe00000u) != 0xffe00000u)
		{
			++position;
			continue;
		}

		int const versionBits = (header >> 19) & 3;
		int const layerBits = (header >> 17) & 3;
		int const bitrateIndex = (header >> 12) & 15;
		int const sampleRateIndex = (header >> 10) & 3;
		int const padding = (header >> 9) & 1;
		if (versionBits == 1 || layerBits != 1 || bitrateIndex == 0 || bitrateIndex == 15 || sampleRateIndex == 3)
		{
			++position;
			continue;
		}

		bool const mpeg1 = versionBits == 3;
		int sampleRate = sampleRates[sampleRateIndex];
		if (versionBits == 2)
			sampleRate /= 2;
		else if (versionBits == 0)
			sampleRate /= 4;
		int const bitrate = (mpeg1 ? bitrateMpeg1Layer3[bitrateIndex] : bitrateMpeg2Layer3[bitrateIndex]) * 1000;
		int const frameLength = (mpeg1 ? 144 : 72) * bitrate / sampleRate + padding;
		if (frameLength <= 0 || position + static_cast<size_t>(frameLength) > data.size())
		{
			++position;
			continue;
		}
		position += static_cast<size_t>(frameLength);
		frameCursor += mpeg1 ? 1152 : 576;
	}
	return frameCursor;
}

FrameIndex mapEncodedOffsetToFrame(SampleState const &sample, S32 offset)
{
	if (offset <= 0 || sample.totalFrames == 0)
		return 0;
	U8 const *data = sample.encodedData.data();
	size_t const size = sample.encodedData.size();
	if (isWave(data, size))
	{
		size_t dataOffset = 0;
		U32 dataLength = 0;
		if (findWaveChunk(data, size, "data", dataOffset, dataLength))
		{
			std::int64_t const relative = (std::max)(static_cast<std::int64_t>(offset) - static_cast<std::int64_t>(dataOffset), static_cast<std::int64_t>(0));
			return (std::min)(sample.totalFrames, static_cast<FrameIndex>(relative / sample.encodedBlockAlign));
		}
	}
	else if (isMp3(data, size))
	{
		return (std::min)(sample.totalFrames, mapMp3OffsetToFrame(sample.encodedData, offset));
	}
	if (size == 0)
		return 0;
	double const ratio = clampValue(static_cast<double>(offset) / static_cast<double>(size), 0.0, 1.0);
	return static_cast<FrameIndex>(ratio * static_cast<double>(sample.totalFrames));
}

U32 mapFrameToEncodedOffset(SampleState const &sample, FrameIndex frame)
{
	if (sample.totalFrames == 0 || sample.encodedData.empty())
		return 0;
	if (isWave(sample.encodedData.data(), sample.encodedData.size()))
	{
		size_t dataOffset = 0;
		U32 dataLength = 0;
		if (findWaveChunk(sample.encodedData.data(), sample.encodedData.size(), "data", dataOffset, dataLength))
		{
			FrameIndex const byteOffset = static_cast<FrameIndex>(dataOffset) + frame * static_cast<FrameIndex>(sample.encodedBlockAlign);
			return static_cast<U32>((std::min)(byteOffset, static_cast<FrameIndex>((std::numeric_limits<U32>::max)())));
		}
	}
	return static_cast<U32>((std::min)(
		static_cast<FrameIndex>(sample.encodedData.size()),
		frame * static_cast<FrameIndex>(sample.encodedData.size()) / sample.totalFrames));
}

juce::Reverb::Parameters getReverbPreset(S32 roomType)
{
	juce::Reverb::Parameters parameters;
	parameters.dryLevel = 0.0f;
	parameters.wetLevel = 1.0f;
	parameters.width = 1.0f;
	parameters.freezeMode = 0.0f;

	switch (roomType)
	{
		case ENVIRONMENT_PADDEDCELL:    parameters.roomSize = 0.08f; parameters.damping = 0.95f; break;
		case ENVIRONMENT_LIVINGROOM:   parameters.roomSize = 0.18f; parameters.damping = 0.80f; break;
		case ENVIRONMENT_ROOM:         parameters.roomSize = 0.28f; parameters.damping = 0.68f; break;
		case ENVIRONMENT_BATHROOM:     parameters.roomSize = 0.35f; parameters.damping = 0.28f; break;
		case ENVIRONMENT_HALLWAY:      parameters.roomSize = 0.48f; parameters.damping = 0.58f; break;
		case ENVIRONMENT_STONECORRIDOR:parameters.roomSize = 0.56f; parameters.damping = 0.35f; break;
		case ENVIRONMENT_AUDITORIUM:   parameters.roomSize = 0.68f; parameters.damping = 0.48f; break;
		case ENVIRONMENT_CONCERTHALL:  parameters.roomSize = 0.76f; parameters.damping = 0.40f; break;
		case ENVIRONMENT_CAVE:         parameters.roomSize = 0.84f; parameters.damping = 0.25f; break;
		case ENVIRONMENT_HANGAR:       parameters.roomSize = 0.88f; parameters.damping = 0.38f; break;
		case ENVIRONMENT_ARENA:        parameters.roomSize = 0.94f; parameters.damping = 0.32f; break;
		case ENVIRONMENT_UNDERWATER:   parameters.roomSize = 0.52f; parameters.damping = 0.98f; break;
		case ENVIRONMENT_MOUNTAINS:
		case ENVIRONMENT_PLAIN:
		case ENVIRONMENT_FOREST:
		case ENVIRONMENT_CITY:         parameters.roomSize = 0.32f; parameters.damping = 0.72f; break;
		default:                       parameters.roomSize = 0.42f; parameters.damping = 0.58f; break;
	}
	return parameters;
}

void applyRoomPreset(DriverState &driver)
{
	driver.reverb.setParameters(getReverbPreset(driver.roomType));
	driver.reverb.reset();
}

S32 requestedChannelCount(S32 channelSpec)
{
	switch (channelSpec)
	{
		case MSS_MC_MONO: return 1;
		case MSS_MC_51_DTS:
		case MSS_MC_51_DISCRETE: return 6;
		case MSS_MC_61_DISCRETE: return 7;
		case MSS_MC_71_DISCRETE: return 8;
		default: return 2;
	}
}

F32 calculateDoppler(DriverState const &driver, SampleState const &sample, Vector3 const &direction)
{
	Vector3 const sourceVelocity = {sample.velocityX, sample.velocityY, sample.velocityZ};
	F32 const speedOfSound = 343.0f;
	F32 const listenerRadial = dot(driver.listenerVelocity, direction);
	F32 const sourceRadial = dot(sourceVelocity, direction);
	F32 const numerator = clampValue(speedOfSound - listenerRadial, speedOfSound * 0.25f, speedOfSound * 4.0f);
	F32 const denominator = clampValue(speedOfSound - sourceRadial, speedOfSound * 0.25f, speedOfSound * 4.0f);
	return clampValue(numerator / denominator, 0.5f, 2.0f);
}

void finishSample(SampleState &sample)
{
	sample.status = SMP_DONE;
	sample.completed.store(true, std::memory_order_release);
}

void mixSample(
	DriverState &driver,
	SampleState &sample,
	float *const *outputs,
	S32 outputChannels,
	S32 numSamples,
	float *wetLeft,
	float *wetRight)
{
	if (sample.status != SMP_PLAYING || sample.totalFrames == 0 || sample.pcm.getNumSamples() == 0)
		return;

	Vector3 const sourcePosition = {sample.positionX, sample.positionY, sample.positionZ};
	Vector3 const relative = subtract(sourcePosition, driver.listenerPosition);
	F32 const distance = length(relative);
	Vector3 const direction = normalize(relative, driver.listenerFront);
	Vector3 const right = normalize(cross(driver.listenerFront, driver.listenerUp), {1.0f, 0.0f, 0.0f});
	F32 const sidePan = clampValue(dot(direction, right), -1.0f, 1.0f);
	F32 const frontness = clampValue((dot(direction, normalize(driver.listenerFront, {0.0f, 0.0f, -1.0f})) + 1.0f) * 0.5f, 0.0f, 1.0f);

	F32 attenuation = 1.0f;
	if (sample.spatialized)
	{
		F32 const maximumDistance = (std::max)(sample.maxDistance, 1.0f);
		F32 const minimumDistance = clampValue(sample.minDistance, 0.0f, maximumDistance);
		if (distance >= maximumDistance)
			attenuation = 0.0f;
		else if (distance > minimumDistance && maximumDistance > minimumDistance)
		{
			F32 const normalizedDistance = (distance - minimumDistance) / (maximumDistance - minimumDistance);
			attenuation = std::pow(1.0f - normalizedDistance, (std::max)(driver.rolloffFactor, 0.05f));
		}
	}

	F32 const blocked = clampValue((std::max)(sample.obstruction, sample.occlusion), 0.0f, 1.0f);
	F32 const blockedGain = clampValue(1.0f - sample.obstruction * 0.35f - sample.occlusion * 0.20f, 0.0f, 1.0f);
	F32 const cutoff = 20000.0f - blocked * 18500.0f;
	F32 const filterAlpha = std::exp(-2.0f * 3.14159265358979323846f * cutoff / static_cast<F32>((std::max)(driver.sampleRate, 1.0)));
	F32 const panLeft = std::sqrt(0.5f * (1.0f - sidePan));
	F32 const panRight = std::sqrt(0.5f * (1.0f + sidePan));
	F32 const spatialVolume = (sample.leftVolume + sample.rightVolume) * 0.5f * attenuation * blockedGain * driver.masterVolume;
	F32 const doppler = sample.spatialized ? calculateDoppler(driver, sample, direction) : 1.0f;
	double const step = clampValue(
		static_cast<double>((std::max)(sample.playbackRate, 1)) / (std::max)(driver.sampleRate, 1.0) * doppler,
		0.0625,
		8.0);

	float const *sourceLeft = sample.pcm.getReadPointer(0);
	float const *sourceRight = sample.sourceChannels > 1 ? sample.pcm.getReadPointer(1) : sourceLeft;
	FrameIndex const loopStart = (std::min)(mapEncodedOffsetToFrame(sample, sample.loopStartOffset), sample.totalFrames - 1);
	FrameIndex const loopEnd = sample.loopEndOffset < 0
		? sample.totalFrames
		: clampValue(mapEncodedOffsetToFrame(sample, sample.loopEndOffset), loopStart + 1, sample.totalFrames);

	for (S32 outputFrame = 0; outputFrame < numSamples; ++outputFrame)
	{
		if (sample.cursorFrame >= static_cast<double>(sample.totalFrames))
		{
			finishSample(sample);
			break;
		}

		FrameIndex const frame0 = static_cast<FrameIndex>(sample.cursorFrame);
		FrameIndex const frame1 = (std::min)(frame0 + 1, sample.totalFrames - 1);
		F32 const fraction = static_cast<F32>(sample.cursorFrame - static_cast<double>(frame0));
		F32 left = sourceLeft[frame0] + (sourceLeft[frame1] - sourceLeft[frame0]) * fraction;
		F32 rightSample = sourceRight[frame0] + (sourceRight[frame1] - sourceRight[frame0]) * fraction;
		sample.filterLeft = left + filterAlpha * (sample.filterLeft - left);
		sample.filterRight = rightSample + filterAlpha * (sample.filterRight - rightSample);
		left = sample.filterLeft;
		rightSample = sample.filterRight;

		if (sample.spatialized)
		{
			F32 const mono = (left + rightSample) * 0.5f;
			F32 const dryGain = spatialVolume * sample.dryLevel * driver.masterDryLevel;
			F32 const frontGain = outputChannels >= 4 ? std::sqrt(frontness) : 1.0f;
			F32 const rearGain = outputChannels >= 4 ? std::sqrt(1.0f - frontness) : 0.0f;
			if (outputChannels > 0 && outputs[0]) outputs[0][outputFrame] += mono * panLeft * dryGain * frontGain;
			if (outputChannels > 1 && outputs[1]) outputs[1][outputFrame] += mono * panRight * dryGain * frontGain;
			if (outputChannels == 4)
			{
				if (outputs[2]) outputs[2][outputFrame] += mono * panLeft * dryGain * rearGain;
				if (outputs[3]) outputs[3][outputFrame] += mono * panRight * dryGain * rearGain;
			}
			else if (outputChannels >= 6)
			{
				if (outputs[4]) outputs[4][outputFrame] += mono * panLeft * dryGain * rearGain;
				if (outputs[5]) outputs[5][outputFrame] += mono * panRight * dryGain * rearGain;
				if (outputChannels >= 8)
				{
					if (outputs[6]) outputs[6][outputFrame] += mono * panLeft * dryGain * 0.5f;
					if (outputs[7]) outputs[7][outputFrame] += mono * panRight * dryGain * 0.5f;
				}
			}
			F32 const wetGain = spatialVolume * sample.wetLevel * driver.masterWetLevel;
			wetLeft[outputFrame] += mono * panLeft * wetGain;
			wetRight[outputFrame] += mono * panRight * wetGain;
		}
		else
		{
			F32 const dryGain = sample.dryLevel * driver.masterDryLevel * driver.masterVolume;
			if (outputChannels > 0 && outputs[0]) outputs[0][outputFrame] += left * sample.leftVolume * dryGain;
			if (outputChannels > 1 && outputs[1]) outputs[1][outputFrame] += rightSample * sample.rightVolume * dryGain;
			F32 const wetGain = sample.wetLevel * driver.masterWetLevel * driver.masterVolume;
			wetLeft[outputFrame] += left * sample.leftVolume * wetGain;
			wetRight[outputFrame] += rightSample * sample.rightVolume * wetGain;
		}

		sample.cursorFrame += step;
		if (sample.loopActive && sample.cursorFrame >= static_cast<double>(loopEnd))
		{
			if (sample.loopCount == 0 || sample.remainingLoops > 1)
			{
				if (sample.remainingLoops > 1)
					--sample.remainingLoops;
				double const overflow = sample.cursorFrame - static_cast<double>(loopEnd);
				sample.cursorFrame = static_cast<double>(loopStart) + std::fmod(overflow, static_cast<double>(loopEnd - loopStart));
			}
			else
			{
				sample.loopActive = false;
			}
		}
	}
}

void DriverState::audioDeviceIOCallbackWithContext(
	float const *const *,
	int,
	float *const *outputChannelData,
	int numOutputChannels,
	int numSamples,
	juce::AudioIODeviceCallbackContext const &)
{
	for (int channel = 0; channel < numOutputChannels; ++channel)
	{
		if (outputChannelData[channel])
			juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
	}

	std::unique_lock<std::recursive_mutex> lock(s_mutex, std::try_to_lock);
	if (!lock.owns_lock() || numOutputChannels <= 0 || numSamples <= 0)
		return;

	if (wetBuffer.getNumSamples() < numSamples)
		wetBuffer.setSize(2, numSamples, false, false, true);
	wetBuffer.clear(0, 0, numSamples);
	wetBuffer.clear(1, 0, numSamples);
	float *wetLeft = wetBuffer.getWritePointer(0);
	float *wetRight = wetBuffer.getWritePointer(1);

	outputChannels = numOutputChannels;
	for (auto &entry : s_samples)
	{
		if (entry.second->driver == handle)
			mixSample(*this, *entry.second, outputChannelData, numOutputChannels, numSamples, wetLeft, wetRight);
	}
	renderSynthSessions(*this, outputChannelData, numOutputChannels, numSamples);

	reverb.processStereo(wetLeft, wetRight, numSamples);
	if (outputChannelData[0]) juce::FloatVectorOperations::add(outputChannelData[0], wetLeft, numSamples);
	if (numOutputChannels > 1 && outputChannelData[1]) juce::FloatVectorOperations::add(outputChannelData[1], wetRight, numSamples);
}

void DriverState::audioDeviceAboutToStart(juce::AudioIODevice *device)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	if (device)
	{
		sampleRate = device->getCurrentSampleRate();
		bufferSize = device->getCurrentBufferSizeSamples();
		outputChannels = device->getActiveOutputChannels().countNumberOfSetBits();
		wetBuffer.setSize(2, (std::max)(bufferSize, 64), false, false, true);
		reverb.setSampleRate(sampleRate);
		applyRoomPreset(*this);
	}
	deviceRunning.store(true, std::memory_order_release);
}

void DriverState::audioDeviceStopped()
{
	deviceRunning.store(false, std::memory_order_release);
}

void DriverState::audioDeviceError(juce::String const &message)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	setError(message.toRawUTF8());
}

void applyMix(SampleState &)
{
	// Mix state is read directly by the JUCE real-time callback.
}

bool createVoice(SampleState &sample)
{
	if (!findDriver(sample.driver) || sample.pcm.getNumSamples() == 0)
	{
		setError("JUCE audio driver or decoded sample is unavailable");
		return false;
	}
	return true;
}

void destroyVoice(SampleState &sample)
{
	sample.status = SMP_DONE;
	sample.completed.store(false, std::memory_order_release);
}

FrameIndex getCurrentFrame(SampleState const &sample)
{
	return (std::min)(sample.totalFrames, static_cast<FrameIndex>((std::max)(sample.cursorFrame, 0.0)));
}

bool startSample(SampleState &sample)
{
	if (sample.pcm.getNumSamples() == 0 || sample.totalFrames == 0 || !findDriver(sample.driver))
	{
		setError("Cannot start a sample before audio data and a JUCE device are assigned");
		return false;
	}
	sample.seekFrame = (std::min)(sample.seekFrame, sample.totalFrames - 1);
	sample.cursorFrame = static_cast<double>(sample.seekFrame);
	sample.remainingLoops = sample.loopCount;
	FrameIndex const loopEnd = sample.loopEndOffset < 0 ? sample.totalFrames : mapEncodedOffsetToFrame(sample, sample.loopEndOffset);
	sample.loopActive = (sample.loopCount == 0 || sample.loopCount > 1) && loopEnd > sample.seekFrame;
	sample.filterLeft = 0.0f;
	sample.filterRight = 0.0f;
	sample.completed.store(false, std::memory_order_release);
	sample.status = SMP_PLAYING;
	return true;
}

void releaseSample(HSAMPLE sampleHandle)
{
	auto const iter = s_samples.find(sampleHandle);
	if (iter == s_samples.end())
		return;
	destroyVoice(*iter->second);
	delete sampleHandle;
	s_samples.erase(iter);
}

void closeStream(HSTREAM streamHandle)
{
	auto const iter = s_streams.find(streamHandle);
	if (iter == s_streams.end())
		return;
	HSAMPLE const sample = iter->second->sample;
	delete streamHandle;
	s_streams.erase(iter);
	releaseSample(sample);
}

void destroyDriver(HDIGDRIVER driverHandle)
{
	auto const iter = s_drivers.find(driverHandle);
	if (iter == s_drivers.end())
		return;
	DriverState &driver = *iter->second;
	driver.deviceManager.removeAudioCallback(&driver);
	driver.deviceManager.closeAudioDevice();
	delete driverHandle;
	s_drivers.erase(iter);
}

bool readStreamFile(char const *filename, std::vector<U8> &data)
{
	if (!s_fileOpenCallback || !s_fileCloseCallback || !s_fileSeekCallback || !s_fileReadCallback)
	{
		setError("SWG audio file callbacks are not installed");
		return false;
	}

	UINTa fileHandle = 0;
	if (!s_fileOpenCallback(filename, &fileHandle))
	{
		s_fileError = AIL_FILE_NOT_FOUND;
		setError("Unable to open streamed audio from the TreeFile system");
		return false;
	}
	S32 const fileLength = s_fileSeekCallback(fileHandle, 0, AIL_FILE_SEEK_END);
	s_fileSeekCallback(fileHandle, 0, AIL_FILE_SEEK_BEGIN);
	if (fileLength <= 0)
	{
		s_fileCloseCallback(fileHandle);
		s_fileError = AIL_CANT_READ_FILE;
		setError("Streamed audio file is empty");
		return false;
	}

	data.resize(static_cast<size_t>(fileLength));
	U32 totalRead = 0;
	while (totalRead < static_cast<U32>(fileLength))
	{
		U32 const bytesRead = s_fileReadCallback(fileHandle, data.data() + totalRead, static_cast<U32>(fileLength) - totalRead);
		if (bytesRead == 0)
			break;
		totalRead += bytesRead;
	}
	s_fileCloseCallback(fileHandle);
	if (totalRead != static_cast<U32>(fileLength))
	{
		data.clear();
		s_fileError = AIL_CANT_READ_FILE;
		setError("Unable to read the complete streamed audio file");
		return false;
	}
	s_fileError = AIL_NO_ERROR;
	return true;
}
}

using namespace JuceMilesNamespace;

std::vector<ClientAudioMidiDevice> ClientAudioMidi::getInputDevices()
{
	std::vector<ClientAudioMidiDevice> result;
	juce::Array<juce::MidiDeviceInfo> const devices = juce::MidiInput::getAvailableDevices();
	result.reserve(static_cast<size_t>(devices.size()));
	for (juce::MidiDeviceInfo const &device : devices)
	{
		ClientAudioMidiDevice item;
		item.name = device.name.toStdString();
		item.identifier = device.identifier.toStdString();
		result.push_back(item);
	}
	return result;
}

bool ClientAudioMidi::openInput(std::string const &identifier, std::string &errorMessage)
{
	closeInput();
	errorMessage.clear();
	if (!s_started || !s_juceInitialiser)
	{
		errorMessage = "JUCE audio is not initialized";
		return false;
	}

	std::unique_ptr<juce::MidiInput> input = juce::MidiInput::openDevice(identifier, &s_midiInputCallback);
	if (!input)
	{
		errorMessage = "Unable to open the selected MIDI input";
		return false;
	}

	input->start();
	{
		std::lock_guard<std::mutex> lock(s_midiMutex);
		s_midiInputIdentifier = identifier;
		s_midiInput = std::move(input);
		s_midiEvents.clear();
	}
	return true;
}

bool ClientAudioMidi::openFirstAvailableInput(std::string &deviceName, std::string &errorMessage)
{
	std::vector<ClientAudioMidiDevice> const devices = getInputDevices();
	if (devices.empty())
	{
		deviceName.clear();
		errorMessage = "No MIDI input device was found; keyboard performance input remains available";
		return false;
	}

	deviceName = devices.front().name;
	return openInput(devices.front().identifier, errorMessage);
}

void ClientAudioMidi::closeInput()
{
	std::unique_ptr<juce::MidiInput> input;
	{
		std::lock_guard<std::mutex> lock(s_midiMutex);
		input = std::move(s_midiInput);
		s_midiInputIdentifier.clear();
		s_midiEvents.clear();
	}
	if (input)
		input->stop();
	clearEvents();
}

bool ClientAudioMidi::isInputOpen()
{
	std::lock_guard<std::mutex> lock(s_midiMutex);
	return s_midiInput.get() != nullptr;
}

std::string ClientAudioMidi::getOpenInputIdentifier()
{
	std::lock_guard<std::mutex> lock(s_midiMutex);
	return s_midiInputIdentifier;
}

bool ClientAudioMidi::pollEvent(ClientAudioMidiEvent &event)
{
	std::lock_guard<std::mutex> lock(s_midiMutex);
	if (s_midiEvents.empty())
		return false;
	event = s_midiEvents.front();
	s_midiEvents.pop_front();
	return true;
}

void ClientAudioMidi::clearEvents()
{
	std::lock_guard<std::mutex> lock(s_midiMutex);
	s_midiEvents.clear();
}

bool ClientAudioMidi::loadMidiSequence(std::string const &filePath, std::vector<ClientAudioMidiEvent> &events, ClientAudioMidiSequenceInfo &info, std::string &errorMessage)
{
	events.clear();
	info = ClientAudioMidiSequenceInfo();
	errorMessage.clear();

	int64 const maximumFileBytes = 4 * 1024 * 1024;
	int const maximumTracks = 64;
	size_t const maximumEvents = 50000;
	double const maximumDurationSeconds = 30.0 * 60.0;
	size_t const maximumEventsPerSecond = 160;

	juce::File const file(juce::String::fromUTF8(filePath.c_str()));
	juce::FileInputStream input(file);
	if (!input.openedOk())
	{
		errorMessage = "Could not open the MIDI file";
		return false;
	}
	if (input.getTotalLength() <= 0 || input.getTotalLength() > maximumFileBytes)
	{
		errorMessage = "MIDI files must be between 1 byte and 4 MiB";
		return false;
	}

	juce::MidiFile midiFile;
	int fileType = -1;
	if (!midiFile.readFrom(input, true, &fileType))
	{
		errorMessage = "The file is not a valid Standard MIDI File";
		return false;
	}
	if (fileType != 0 && fileType != 1)
	{
		errorMessage = "Only Standard MIDI File formats 0 and 1 are supported";
		return false;
	}
	if (midiFile.getNumTracks() < 1 || midiFile.getNumTracks() > maximumTracks)
	{
		errorMessage = "MIDI files must contain between 1 and 64 tracks";
		return false;
	}

	midiFile.convertTimestampTicksToSeconds();
	bool hasNoteOn = false;
	for (int trackIndex = 0; trackIndex < midiFile.getNumTracks(); ++trackIndex)
	{
		juce::MidiMessageSequence const * const track = midiFile.getTrack(trackIndex);
		if (!track)
			continue;
		for (int eventIndex = 0; eventIndex < track->getNumEvents(); ++eventIndex)
		{
			juce::MidiMessage const &message = track->getEventPointer(eventIndex)->message;
			ClientAudioMidiEvent event = {};
			if (message.isNoteOn())
			{
				event.type = ClientAudioMidiEvent::T_noteOn;
				event.note = message.getNoteNumber();
				event.value = message.getVelocity();
				hasNoteOn = true;
			}
			else if (message.isNoteOff())
			{
				event.type = ClientAudioMidiEvent::T_noteOff;
				event.note = message.getNoteNumber();
				event.value = message.getVelocity();
			}
			else if (message.isController() && (message.getControllerNumber() == 64 || message.getControllerNumber() == 120 || message.getControllerNumber() == 123))
			{
				event.type = ClientAudioMidiEvent::T_controlChange;
				event.note = message.getControllerNumber();
				event.value = message.getControllerValue();
			}
			else
				continue;

			event.channel = message.getChannel();
			event.timestampSeconds = message.getTimeStamp();
			if (!std::isfinite(event.timestampSeconds) || event.timestampSeconds < 0.0 || event.timestampSeconds > maximumDurationSeconds)
			{
				errorMessage = "MIDI duration must not exceed 30 minutes";
				events.clear();
				return false;
			}
			events.push_back(event);
			if (events.size() > maximumEvents)
			{
				errorMessage = "MIDI files may contain at most 50,000 playable events";
				events.clear();
				return false;
			}
		}
	}

	if (!hasNoteOn)
	{
		errorMessage = "The MIDI file does not contain any playable notes";
		events.clear();
		return false;
	}

	std::stable_sort(events.begin(), events.end(), [](ClientAudioMidiEvent const &a, ClientAudioMidiEvent const &b)
	{
		return a.timestampSeconds < b.timestampSeconds;
	});
	for (size_t first = 0, current = 0; current < events.size(); ++current)
	{
		while (first < current && events[current].timestampSeconds - events[first].timestampSeconds >= 1.0)
			++first;
		if (current - first + 1 > maximumEventsPerSecond)
		{
			errorMessage = "MIDI event density exceeds the multiplayer limit of 160 events per second";
			events.clear();
			return false;
		}
	}

	info.fileType = fileType;
	info.trackCount = midiFile.getNumTracks();
	info.eventCount = static_cast<int>(events.size());
	info.durationSeconds = events.back().timestampSeconds;
	return true;
}

void ClientAudioMidi::startSynthSession(unsigned long long performerId, int instrumentId)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SynthSession &session = s_synthSessions[performerId];
	session = SynthSession();
	session.instrumentId = clampValue(instrumentId, 1, 14);
}

void ClientAudioMidi::stopSynthSession(unsigned long long performerId)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	s_synthSessions.erase(performerId);
}

bool ClientAudioMidi::hasSynthSession(unsigned long long performerId)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	return s_synthSessions.find(performerId) != s_synthSessions.end();
}

void ClientAudioMidi::synthNoteOn(unsigned long long performerId, int channel, int note, int velocity)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SynthSessionMap::iterator const sessionIterator = s_synthSessions.find(performerId);
	if (sessionIterator == s_synthSessions.end())
		return;

	SynthSession &session = sessionIterator->second;
	channel = clampValue(channel, 0, 15);
	note = clampValue(note, 0, 127);
	velocity = clampValue(velocity, 1, 127);
	SynthVoice *selected = nullptr;
	for (SynthVoice &voice : session.voices)
	{
		if (!voice.active)
		{
			selected = &voice;
			break;
		}
		if (!selected || voice.age < selected->age)
			selected = &voice;
	}
	if (!selected)
		return;

	*selected = SynthVoice();
	selected->active = true;
	selected->channel = channel;
	selected->note = note;
	selected->velocity = static_cast<float>(velocity) / 127.0f;
	selected->age = session.nextVoiceAge++;
	selected->noiseState ^= static_cast<std::uint32_t>(note * 2654435761u) ^ static_cast<std::uint32_t>(selected->age);
}

void ClientAudioMidi::synthNoteOff(unsigned long long performerId, int channel, int note)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SynthSessionMap::iterator const sessionIterator = s_synthSessions.find(performerId);
	if (sessionIterator == s_synthSessions.end())
		return;

	SynthSession &session = sessionIterator->second;
	channel = clampValue(channel, 0, 15);
	note = clampValue(note, 0, 127);
	for (SynthVoice &voice : session.voices)
	{
		if (!voice.active || voice.channel != channel || voice.note != note || voice.stage == SES_release)
			continue;
		if (session.sustain[static_cast<size_t>(channel)])
			voice.heldBySustain = true;
		else
			releaseSynthVoice(voice);
	}
}

void ClientAudioMidi::synthSustain(unsigned long long performerId, int channel, bool enabled)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SynthSessionMap::iterator const sessionIterator = s_synthSessions.find(performerId);
	if (sessionIterator == s_synthSessions.end())
		return;

	SynthSession &session = sessionIterator->second;
	channel = clampValue(channel, 0, 15);
	session.sustain[static_cast<size_t>(channel)] = enabled;
	if (!enabled)
		for (SynthVoice &voice : session.voices)
			if (voice.active && voice.channel == channel && voice.heldBySustain)
				releaseSynthVoice(voice);
}

void ClientAudioMidi::synthAllNotesOff(unsigned long long performerId)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SynthSessionMap::iterator const sessionIterator = s_synthSessions.find(performerId);
	if (sessionIterator == s_synthSessions.end())
		return;
	for (SynthVoice &voice : sessionIterator->second.voices)
		releaseSynthVoice(voice);
	sessionIterator->second.sustain.fill(false);
}

char const *ClientAudioMidi::getSynthPatchName(int instrumentId)
{
	return getSynthPatch(instrumentId).name;
}

int ClientAudioMidi::getLoadedSynthSampleCount()
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	return s_loadedSynthSampleCount;
}

extern "C"
{
S32 AILCALL AIL_startup(void)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	if (!s_started)
	{
		s_juceInitialiser = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
		s_formatManager.registerBasicFormats();
		discoverSynthSampleBank();
		s_preferences[DIG_MIXER_CHANNELS] = 64;
		s_started = true;
	}
	s_lastError.clear();
	return 1;
}

void AILCALL AIL_shutdown(void)
{
	ClientAudioMidi::closeInput();
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	s_synthSessions.clear();
	while (!s_streams.empty()) closeStream(s_streams.begin()->first);
	while (!s_samples.empty()) releaseSample(s_samples.begin()->first);
	while (!s_drivers.empty()) destroyDriver(s_drivers.begin()->first);
	s_formatManager.clearFormats();
	s_juceInitialiser.reset();
	s_started = false;
}

SINTa AILCALL AIL_get_preference(U32 number)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	auto const iter = s_preferences.find(number);
	return iter == s_preferences.end() ? 0 : iter->second;
}

SINTa AILCALL AIL_set_preference(U32 number, SINTa value)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SINTa const previous = AIL_get_preference(number);
	s_preferences[number] = value;
	return previous;
}

char *AILCALL AIL_last_error(void)
{
	return s_lastError.empty() ? const_cast<char *>("") : const_cast<char *>(s_lastError.c_str());
}

void AILCALL AIL_lock(void) { s_mutex.lock(); }
void AILCALL AIL_unlock(void) { s_mutex.unlock(); }

char *AILCALL AIL_set_redist_directory(char const *directory)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	s_redistDirectory = directory ? directory : "";
	return const_cast<char *>(s_redistDirectory.c_str());
}

HDIGDRIVER AILCALL AIL_open_digital_driver(U32 frequency, S32, S32 channel, U32)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	if (!s_started) AIL_startup();

	auto state = std::make_unique<DriverState>();
	state->channelSpec = static_cast<MSS_MC_SPEC>(channel);
	juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
	preferredSetup.sampleRate = frequency ? static_cast<double>(frequency) : 0.0;
	juce::String error = state->deviceManager.initialise(
		0,
		requestedChannelCount(channel),
		nullptr,
		true,
		juce::String(),
		&preferredSetup);
	if (error.isNotEmpty())
	{
		error = state->deviceManager.initialiseWithDefaultDevices(0, 2);
		state->channelSpec = MSS_MC_STEREO;
	}
	if (error.isNotEmpty() || !state->deviceManager.getCurrentAudioDevice())
	{
		setError(error.isNotEmpty() ? error.toRawUTF8() : "JUCE could not open the default output device");
		return nullptr;
	}

	HDIGDRIVER token = new _DIG_DRIVER;
	std::memset(token, 0, sizeof(*token));
	state->handle = token;
	auto inserted = s_drivers.emplace(token, std::move(state));
	DriverState &driver = *inserted.first->second;
	driver.deviceManager.addAudioCallback(&driver);
	s_lastError.clear();
	return token;
}

void AILCALL AIL_close_digital_driver(HDIGDRIVER driver)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	destroyDriver(driver);
}

S32 AILCALL AIL_digital_CPU_percent(HDIGDRIVER driverHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	return driver ? static_cast<S32>(driver->deviceManager.getCpuUsage() * 100.0) : 0;
}

S32 AILCALL AIL_digital_latency(HDIGDRIVER driverHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	return driver && driver->sampleRate > 0.0 ? static_cast<S32>(driver->bufferSize * 1000.0 / driver->sampleRate) : 0;
}

void AILCALL AIL_digital_configuration(HDIGDRIVER driverHandle, S32 *rate, S32 *format, char *description)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	if (rate) *rate = driver ? static_cast<S32>(driver->sampleRate) : 0;
	if (format) *format = driver ? DIG_F_16BITS_MASK | (driver->outputChannels > 1 ? DIG_F_STEREO_MASK : 0) : 0;
	if (description) std::strcpy(description, driver ? "JUCE 8.0.14 / Windows Audio" : "JUCE unavailable");
}

void AILCALL AIL_set_digital_master_volume_level(HDIGDRIVER driverHandle, F32 volume)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	if (driver) driver->masterVolume = clampValue(volume, 0.0f, 1.0f);
}

F32 AILCALL AIL_digital_master_volume_level(HDIGDRIVER driverHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	return driver ? driver->masterVolume : 0.0f;
}

void AILCALL AIL_set_digital_master_reverb_levels(HDIGDRIVER driverHandle, F32 dry, F32 wet)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	if (driver)
	{
		driver->masterDryLevel = clampValue(dry, 0.0f, 1.0f);
		driver->masterWetLevel = clampValue(wet, 0.0f, 1.0f);
	}
}

void AILCALL AIL_digital_master_reverb_levels(HDIGDRIVER driverHandle, F32 *dry, F32 *wet)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	if (dry) *dry = driver ? driver->masterDryLevel : 0.0f;
	if (wet) *wet = driver ? driver->masterWetLevel : 0.0f;
}

HSAMPLE AILCALL AIL_allocate_sample_handle(HDIGDRIVER driver)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	if (!findDriver(driver)) return nullptr;
	HSAMPLE token = new _SAMPLE;
	std::memset(token, 0, sizeof(*token));
	s_samples.emplace(token, std::make_unique<SampleState>(driver));
	return token;
}

void AILCALL AIL_release_sample_handle(HSAMPLE sample)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	releaseSample(sample);
}

S32 AILCALL AIL_set_named_sample_file(HSAMPLE sampleHandle, C8 const *, void const *fileImage, U32 fileSize, S32)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (!sample || !fileImage || fileSize == 0)
	{
		setError("Cannot assign an empty audio file");
		return 0;
	}
	destroyVoice(*sample);
	sample->encodedData.assign(static_cast<U8 const *>(fileImage), static_cast<U8 const *>(fileImage) + fileSize);
	return decodeAudio(*sample) && createVoice(*sample) ? 1 : 0;
}

S32 AILCALL AIL_set_sample_file(HSAMPLE sample, void const *fileImage, S32 block)
{
	if (!fileImage) return 0;
	U8 const *data = static_cast<U8 const *>(fileImage);
	if (!isWave(data, 12))
	{
		setError("AIL_set_sample_file requires RIFF/WAVE data in the SWG JUCE backend");
		return 0;
	}
	return AIL_set_named_sample_file(sample, ".wav", fileImage, readU32(data + 4) + 8, block);
}

void AILCALL AIL_start_sample(HSAMPLE sampleHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample) startSample(*sample);
}

void AILCALL AIL_stop_sample(HSAMPLE sampleHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample && sample->status == SMP_PLAYING) sample->status = SMP_STOPPED;
}

void AILCALL AIL_end_sample(HSAMPLE sampleHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample)
	{
		sample->status = SMP_DONE;
		sample->completed.store(false, std::memory_order_release);
	}
}

U32 AILCALL AIL_sample_status(HSAMPLE sampleHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	return sample ? sample->status : SMP_FREE;
}

S32 AILCALL AIL_active_sample_count(HDIGDRIVER driver)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	S32 count = 0;
	for (auto const &entry : s_samples)
		if (entry.second->driver == driver && entry.second->status == SMP_PLAYING) ++count;
	return count;
}

void AILCALL AIL_set_sample_playback_rate(HSAMPLE sampleHandle, S32 playbackRate)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample) sample->playbackRate = playbackRate;
}

S32 AILCALL AIL_sample_playback_rate(HSAMPLE sampleHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	return sample ? sample->playbackRate : 0;
}

void AILCALL AIL_set_sample_volume_levels(HSAMPLE sampleHandle, F32 left, F32 right)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample) { sample->leftVolume = left; sample->rightVolume = right; }
}

void AILCALL AIL_sample_volume_levels(HSAMPLE sampleHandle, F32 *left, F32 *right)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (left) *left = sample ? sample->leftVolume : 0.0f;
	if (right) *right = sample ? sample->rightVolume : 0.0f;
}

void AILCALL AIL_set_sample_reverb_levels(HSAMPLE sampleHandle, F32 dry, F32 wet)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample) { sample->dryLevel = clampValue(dry, 0.0f, 1.0f); sample->wetLevel = clampValue(wet, 0.0f, 1.0f); }
}

void AILCALL AIL_sample_reverb_levels(HSAMPLE sampleHandle, F32 *dry, F32 *wet)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (dry) *dry = sample ? sample->dryLevel : 0.0f;
	if (wet) *wet = sample ? sample->wetLevel : 0.0f;
}

void AILCALL AIL_set_sample_loop_count(HSAMPLE sampleHandle, S32 count)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample) sample->loopCount = count;
}

S32 AILCALL AIL_sample_loop_count(HSAMPLE sampleHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	return sample ? sample->loopCount : 0;
}

void AILCALL AIL_set_sample_loop_block(HSAMPLE sampleHandle, S32 startOffset, S32 endOffset)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample) { sample->loopStartOffset = startOffset; sample->loopEndOffset = endOffset; }
}

AILSAMPLECB AILCALL AIL_register_EOS_callback(HSAMPLE sampleHandle, AILSAMPLECB callback)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (!sample) return nullptr;
	AILSAMPLECB const previous = sample->eosCallback;
	sample->eosCallback = callback;
	return previous;
}

void AILCALL AIL_set_sample_ms_position(HSAMPLE sampleHandle, S32 milliseconds)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (!sample || sample->sourceSampleRate <= 0) return;
	sample->seekFrame = clampValue<FrameIndex>(
		static_cast<FrameIndex>((std::max)(milliseconds, 0)) * sample->sourceSampleRate / 1000,
		0,
		sample->totalFrames ? sample->totalFrames - 1 : 0);
	if (sample->status == SMP_PLAYING) startSample(*sample); else sample->cursorFrame = static_cast<double>(sample->seekFrame);
}

void AILCALL AIL_sample_ms_position(HSAMPLE sampleHandle, S32 *totalMilliseconds, S32 *currentMilliseconds)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (totalMilliseconds) *totalMilliseconds = sample && sample->sourceSampleRate ? static_cast<S32>(sample->totalFrames * 1000 / sample->sourceSampleRate) : 0;
	if (currentMilliseconds) *currentMilliseconds = sample && sample->sourceSampleRate ? static_cast<S32>(getCurrentFrame(*sample) * 1000 / sample->sourceSampleRate) : 0;
}

void AILCALL AIL_set_sample_position(HSAMPLE sampleHandle, U32 position)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (!sample) return;
	sample->seekFrame = mapEncodedOffsetToFrame(*sample, static_cast<S32>(position));
	if (sample->status == SMP_PLAYING) startSample(*sample); else sample->cursorFrame = static_cast<double>(sample->seekFrame);
}

U32 AILCALL AIL_sample_position(HSAMPLE sampleHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	return sample ? mapFrameToEncodedOffset(*sample, getCurrentFrame(*sample)) : 0;
}

void AILCALL AIL_set_sample_3D_position(HSAMPLE sampleHandle, F32 x, F32 y, F32 z)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample) { sample->spatialized = true; sample->positionX = x; sample->positionY = y; sample->positionZ = z; }
}

void AILCALL AIL_set_sample_3D_velocity_vector(HSAMPLE sampleHandle, F32 x, F32 y, F32 z)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample) { sample->velocityX = x; sample->velocityY = y; sample->velocityZ = z; }
}

void AILCALL AIL_set_sample_3D_distances(HSAMPLE sampleHandle, F32 maxDistance, F32 minDistance, S32)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample)
	{
		sample->maxDistance = (std::max)(maxDistance, 1.0f);
		sample->minDistance = clampValue(minDistance, 0.0f, sample->maxDistance);
		sample->spatialized = true;
	}
}

void AILCALL AIL_set_sample_obstruction(HSAMPLE sampleHandle, F32 value)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample) sample->obstruction = clampValue(value, 0.0f, 1.0f);
}

void AILCALL AIL_set_sample_occlusion(HSAMPLE sampleHandle, F32 value)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(sampleHandle);
	if (sample) sample->occlusion = clampValue(value, 0.0f, 1.0f);
}

HSTREAM AILCALL AIL_open_stream(HDIGDRIVER driver, char const *filename, S32)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	std::vector<U8> fileData;
	if (!findDriver(driver) || !filename || !readStreamFile(filename, fileData)) return nullptr;
	HSAMPLE const sampleHandle = AIL_allocate_sample_handle(driver);
	char const *extension = std::strrchr(filename, '.');
	if (!sampleHandle || !AIL_set_named_sample_file(sampleHandle, extension ? extension : "", fileData.data(), static_cast<U32>(fileData.size()), 0))
	{
		if (sampleHandle) AIL_release_sample_handle(sampleHandle);
		return nullptr;
	}
	HSTREAM token = new _STREAM;
	std::memset(token, 0, sizeof(*token));
	auto stream = std::make_unique<StreamState>();
	stream->sample = sampleHandle;
	stream->filename = filename;
	s_streams.emplace(token, std::move(stream));
	findSample(sampleHandle)->ownerStream = token;
	return token;
}

void AILCALL AIL_close_stream(HSTREAM stream)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	closeStream(stream);
}

HSAMPLE AILCALL AIL_stream_sample_handle(HSTREAM streamHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	StreamState *stream = findStream(streamHandle);
	return stream ? stream->sample : nullptr;
}

void AILCALL AIL_start_stream(HSTREAM stream)
{
	HSAMPLE const sample = AIL_stream_sample_handle(stream);
	if (sample) AIL_start_sample(sample);
}

void AILCALL AIL_pause_stream(HSTREAM stream, S32 onoff)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	SampleState *sample = findSample(AIL_stream_sample_handle(stream));
	if (!sample) return;
	if (onoff) sample->status = SMP_STOPPED;
	else if (sample->status == SMP_STOPPED) sample->status = SMP_PLAYING;
}

void AILCALL AIL_set_stream_loop_count(HSTREAM stream, S32 count) { AIL_set_sample_loop_count(AIL_stream_sample_handle(stream), count); }
void AILCALL AIL_set_stream_loop_block(HSTREAM stream, S32 start, S32 end) { AIL_set_sample_loop_block(AIL_stream_sample_handle(stream), start, end); }
S32 AILCALL AIL_stream_status(HSTREAM stream) { return static_cast<S32>(AIL_sample_status(AIL_stream_sample_handle(stream))); }

AILSTREAMCB AILCALL AIL_register_stream_callback(HSTREAM streamHandle, AILSTREAMCB callback)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	StreamState *stream = findStream(streamHandle);
	if (!stream) return nullptr;
	AILSTREAMCB const previous = stream->callback;
	stream->callback = callback;
	return previous;
}

void AILCALL AIL_set_stream_ms_position(HSTREAM stream, S32 milliseconds) { AIL_set_sample_ms_position(AIL_stream_sample_handle(stream), milliseconds); }
void AILCALL AIL_stream_ms_position(HSTREAM stream, S32 *total, S32 *current) { AIL_sample_ms_position(AIL_stream_sample_handle(stream), total, current); }

void AILCALL AIL_set_file_callbacks(AIL_file_open_callback openCallback, AIL_file_close_callback closeCallback, AIL_file_seek_callback seekCallback, AIL_file_read_callback readCallback)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	s_fileOpenCallback = openCallback;
	s_fileCloseCallback = closeCallback;
	s_fileSeekCallback = seekCallback;
	s_fileReadCallback = readCallback;
}

S32 AILCALL AIL_file_error(void) { return s_fileError; }

S32 AILCALL AIL_file_type(void const *fileImage, U32 fileSize)
{
	if (!fileImage) return AILFILETYPE_UNKNOWN;
	U8 const *data = static_cast<U8 const *>(fileImage);
	if (isWave(data, fileSize)) return AILFILETYPE_PCM_WAV;
	if (isOgg(data, fileSize)) return AILFILETYPE_OGG_VORBIS;
	if (isMp3(data, fileSize)) return AILFILETYPE_MPEG_L3_AUDIO;
	return AILFILETYPE_UNKNOWN;
}

S32 AILCALL AIL_WAV_info(void const *fileImage, AILSOUNDINFO *info)
{
	if (!fileImage || !info) return 0;
	U8 const *data = static_cast<U8 const *>(fileImage);
	U32 const fileSize = isWave(data, 12) ? readU32(data + 4) + 8 : 0;
	if (!fileSize) return 0;
	size_t formatOffset = 0;
	size_t dataOffset = 0;
	U32 formatLength = 0;
	U32 dataLength = 0;
	if (!findWaveChunk(data, fileSize, "fmt ", formatOffset, formatLength) || formatLength < 16 || !findWaveChunk(data, fileSize, "data", dataOffset, dataLength)) return 0;
	std::memset(info, 0, sizeof(*info));
	U16 const waveFormat = readU16(data + formatOffset);
	info->channels = readU16(data + formatOffset + 2);
	info->rate = readU32(data + formatOffset + 4);
	info->block_size = readU16(data + formatOffset + 12);
	info->bits = readU16(data + formatOffset + 14);
	info->data_ptr = data + dataOffset;
	info->data_len = dataLength;
	info->samples = info->block_size ? dataLength / info->block_size : 0;
	info->initial_ptr = fileImage;
	info->format = (info->channels > 1 ? DIG_F_STEREO_MASK : 0) | (info->bits == 16 ? DIG_F_16BITS_MASK : 0);
	if (waveFormat != WAVE_FORMAT_PCM && waveFormat != 3u) info->format |= DIG_F_ADPCM_MASK;
	return 1;
}

void AILCALL AIL_set_3D_rolloff_factor(HDIGDRIVER driverHandle, F32 factor)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	if (driver) driver->rolloffFactor = factor;
}

void AILCALL AIL_set_listener_3D_position(HDIGDRIVER driverHandle, F32 x, F32 y, F32 z)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	if (driver) driver->listenerPosition = {x, y, z};
}

void AILCALL AIL_set_listener_3D_velocity_vector(HDIGDRIVER driverHandle, F32 x, F32 y, F32 z)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	if (driver) driver->listenerVelocity = {x, y, z};
}

void AILCALL AIL_set_listener_3D_orientation(HDIGDRIVER driverHandle, F32 faceX, F32 faceY, F32 faceZ, F32 upX, F32 upY, F32 upZ)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	if (driver)
	{
		driver->listenerFront = normalize({faceX, faceY, faceZ}, {0.0f, 0.0f, -1.0f});
		driver->listenerUp = normalize({upX, upY, upZ}, {0.0f, 1.0f, 0.0f});
	}
}

void AILCALL AIL_set_room_type(HDIGDRIVER driverHandle, S32 roomType)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	if (driver) { driver->roomType = roomType; applyRoomPreset(*driver); }
}

S32 AILCALL AIL_room_type(HDIGDRIVER driverHandle)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	return driver ? driver->roomType : ENVIRONMENT_GENERIC;
}

MSSVECTOR3D *AILCALL AIL_speaker_configuration(HDIGDRIVER driverHandle, S32 *physicalChannels, S32 *logicalChannels, F32 *falloffPower, MSS_MC_SPEC *channelSpec)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	DriverState *driver = findDriver(driverHandle);
	if (physicalChannels) *physicalChannels = driver ? driver->outputChannels : 0;
	if (logicalChannels) *logicalChannels = driver ? driver->outputChannels : 0;
	if (falloffPower) *falloffPower = driver ? driver->rolloffFactor : 1.0f;
	if (channelSpec) *channelSpec = driver ? driver->channelSpec : MSS_MC_STEREO;
	return nullptr;
}

U32 AILCALL AIL_get_timer_highest_delay(void) { return 0; }

void AILCALL AIL_serve(void)
{
	std::lock_guard<std::recursive_mutex> lock(s_mutex);
	std::vector<HSAMPLE> completed;
	for (auto const &entry : s_samples)
		if (entry.second->completed.exchange(false, std::memory_order_acq_rel)) completed.push_back(entry.first);
	for (HSAMPLE sampleHandle : completed)
	{
		SampleState *sample = findSample(sampleHandle);
		if (!sample) continue;
		if (sample->ownerStream)
		{
			StreamState *stream = findStream(sample->ownerStream);
			if (stream && stream->callback) stream->callback(sample->ownerStream);
		}
		else if (sample->eosCallback)
		{
			sample->eosCallback(sampleHandle);
		}
	}
}
}

#endif
