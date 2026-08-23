#ifndef INCLUDED_TcgCompatibilityProtocol_H
#define INCLUDED_TcgCompatibilityProtocol_H

#include <windows.h>

#include <cstddef>
#include <cstdint>

// Pointer-free, fixed-layout transport shared by the x64 game adapter and the
// Win32 process that owns the final 32-bit SWGTCG.dll.  Do not place native
// handles or pointers in this structure: their widths differ across the two
// processes.
namespace TcgCompatibilityProtocol
{
	constexpr std::uint32_t Magic = 0x31474354u; // "TCG1"
	constexpr std::uint32_t Version = 1u;
	constexpr std::uint32_t MaximumWindows = 8u;
	constexpr std::uint32_t MaximumCommands = 1024u;
	constexpr std::uint32_t MaximumCallbacks = 16u;
	constexpr std::uint32_t MaximumTitleBytes = 256u;
	constexpr std::uint32_t MaximumUrlBytes = 4096u;
	constexpr std::uint32_t MaximumPostBytes = 32768u;
	constexpr std::uint32_t MaximumFailureBytes = 512u;
	constexpr std::uint32_t MaximumFramePixelBytes = 48u * 1024u * 1024u;

	enum LifecycleState : std::int32_t
	{
		LifecycleEmpty = 0,
		LifecycleStarting = 1,
		LifecycleReady = 2,
		LifecycleFailed = 3,
		LifecycleStopped = 4
	};

	enum CommandType : std::uint32_t
	{
		CommandNone = 0,
		CommandSetWindowState,
		CommandMusicCompletion,
		CommandSetFocus,
		CommandSetLocation,
		CommandSetSize,
		CommandClose,
		CommandMouse,
		CommandMouseWheel,
		CommandKey,
		CommandShutdown
	};

	enum CallbackType : std::uint32_t
	{
		CallbackNone = 0,
		CallbackNavigate,
		CallbackNavigateWithPost,
		CallbackSetSoundVolume,
		CallbackSetMusicVolume,
		CallbackStopAllSounds,
		CallbackSetWindowState
	};

#pragma pack(push, 4)
	struct Command
	{
		std::uint32_t type;
		std::uint32_t windowId;
		std::int32_t values[12];
	};

	struct Callback
	{
		std::uint32_t type;
		std::int32_t value;
		std::uint32_t urlBytes;
		std::uint32_t postBytes;
		char url[MaximumUrlBytes];
		char postData[MaximumPostBytes];
	};

	struct WindowRecord
	{
		std::uint32_t id;
		std::int32_t x;
		std::int32_t y;
		std::uint32_t width;
		std::uint32_t height;
		std::uint32_t stride;
		std::uint32_t surfaceOffset;
		std::uint32_t surfaceBytes;
		std::uint32_t minimumWidth;
		std::uint32_t minimumHeight;
		std::uint32_t maximumWidth;
		std::uint32_t maximumHeight;
		std::uint32_t canGetFocus;
		char title[MaximumTitleBytes];
	};

	struct Frame
	{
		std::uint32_t windowCount;
		std::uint32_t pixelBytes;
		std::uint32_t captureWindowId;
		std::uint32_t reserved;
		WindowRecord windows[MaximumWindows];
		std::uint8_t pixels[MaximumFramePixelBytes];
	};

	struct SharedState
	{
		std::uint32_t magic;
		std::uint32_t version;
		std::uint32_t structureBytes;
		std::uint32_t parentProcessId;
		volatile LONG lifecycle;
		volatile LONG failureCode;
		volatile LONG hostProcessId;
		volatile LONG activeFrame;
		volatile LONG frameSequence;
		volatile LONG commandWrite;
		volatile LONG commandRead;
		volatile LONG callbackWrite;
		volatile LONG callbackRead;
		char failureMessage[MaximumFailureBytes];
		Command commands[MaximumCommands];
		Callback callbacks[MaximumCallbacks];
		Frame frames[2];
	};
#pragma pack(pop)

	constexpr std::size_t SharedStateBytes = sizeof(SharedState);

	static_assert(sizeof(Command) == 56u, "TCG bridge command layout changed");
	static_assert(sizeof(Callback) == 36880u, "TCG bridge callback layout changed");
	static_assert(sizeof(WindowRecord) == 308u, "TCG bridge window-record layout changed");
	static_assert(sizeof(Frame) == 50334128u, "TCG bridge frame layout changed");
	static_assert(offsetof(SharedState, frames) == 647988u, "TCG bridge frame offset changed");
	static_assert(SharedStateBytes == 101316244u, "TCG bridge shared-state layout changed");
	static_assert(SharedStateBytes < 128u * 1024u * 1024u, "TCG bridge mapping grew unexpectedly");
}

#endif
