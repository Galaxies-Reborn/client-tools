#ifndef INCLUDED_BrowserCompatibilityProtocol_H
#define INCLUDED_BrowserCompatibilityProtocol_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

// This transport is shared verbatim by a 64-bit game process and the 32-bit
// legacy Mozilla broker.  It must never contain pointers, size_t, HANDLE, bool,
// wchar_t, compiler-generated vtables, or any other bitness-dependent field.
namespace BrowserCompatibilityProtocol
{
	constexpr std::uint32_t Magic = 0x31524257u; // "WBR1"
	constexpr std::uint32_t Version = 1u;
	constexpr std::uint32_t MaximumWindows = 8u;
	constexpr std::uint32_t MaximumCommands = 64u;
	constexpr std::uint32_t MaximumUriBytes = 4096u;
	constexpr std::uint32_t MaximumPostBytes = 32768u;
	constexpr std::uint32_t MaximumStatusUnits = 512u;
	constexpr std::uint32_t MaximumUserAgentBytes = 128u;
	constexpr std::uint32_t MaximumFailureBytes = 512u;
	constexpr std::uint32_t MaximumFramePixelBytes = 48u * 1024u * 1024u;
	constexpr std::uint32_t MaximumSurfaceDimension = 8192u;

	constexpr bool isValidSurfaceSize(std::uint32_t width, std::uint32_t height)
	{
		return width != 0u && height != 0u && width <= MaximumSurfaceDimension &&
			height <= MaximumSurfaceDimension &&
			static_cast<std::uint64_t>(width) * height * 4u <= MaximumFramePixelBytes;
	}

	enum LifecycleState : std::int32_t
	{
		LifecycleEmpty = 0,
		LifecycleStarting = 1,
		LifecycleReady = 2,
		LifecycleFailed = 3,
		LifecycleStopping = 4,
		LifecycleStopped = 5
	};

	enum WindowLifecycle : std::uint32_t
	{
		WindowEmpty = 0u,
		WindowCreating = 1u,
		WindowReady = 2u,
		WindowTombstone = 3u,
		WindowFailed = 4u
	};

	enum CommandType : std::uint32_t
	{
		CommandNone = 0u,
		CommandCreateWindow,
		CommandDestroyWindow,
		CommandSetSize,
		CommandSetFocus,
		CommandSetRenderOnComplete,
		CommandNavigate,
		CommandNavigateStop,
		CommandNavigateBack,
		CommandNavigateForward,
		CommandReload,
		CommandLeftMouseDown,
		CommandLeftMouseUp,
		CommandMiddleMouseDown,
		CommandMiddleMouseUp,
		CommandRightMouseDown,
		CommandRightMouseUp,
		CommandMouseMove,
		CommandMouseWheel,
		CommandKeyPress,
		CommandBrowserCommand,
		CommandEnableMemoryCache,
		CommandEnableDiskCache,
		CommandSetUserAgent,
		CommandShutdown
	};

	enum CommandFlags : std::uint32_t
	{
		CommandFlagNone = 0u,
		CommandFlagHasPostData = 1u << 0u
	};

	enum SurfaceFormat : std::uint32_t
	{
		SurfaceFormatNone = 0u,
		SurfaceFormatBgra8 = 1u
	};

#pragma pack(push, 4)
	struct Command
	{
		std::uint32_t type;
		std::uint32_t windowId;
		std::uint32_t flags;
		std::uint32_t uriBytes;
		std::uint32_t postBytes;
		std::int32_t values[8];
		char uri[MaximumUriBytes];
		std::uint8_t postData[MaximumPostBytes];
	};

	struct WindowState
	{
		volatile std::int32_t sequence;
		std::uint32_t windowId;
		std::uint32_t lifecycle;
		std::uint32_t width;
		std::uint32_t height;
		std::uint32_t canNavigateBack;
		std::uint32_t canNavigateForward;
		std::uint32_t isLoading;
		std::uint32_t progressBits;
		std::uint32_t hasCaret;
		std::int32_t caretX;
		std::int32_t caretY;
		std::int32_t caretWidth;
		std::int32_t caretHeight;
		std::uint32_t uriGeneration;
		std::uint32_t statusGeneration;
		std::uint32_t progressGeneration;
		std::uint32_t uriBytes;
		char uri[MaximumUriBytes];
		std::uint16_t status[MaximumStatusUnits];
	};

	struct SurfaceRecord
	{
		std::uint32_t windowId;
		std::uint32_t width;
		std::uint32_t height;
		std::uint32_t stride;
		std::uint32_t bytesPerRow;
		std::uint32_t format;
		std::uint32_t surfaceOffset;
		std::uint32_t surfaceBytes;
	};

	struct Frame
	{
		volatile std::int32_t sequence;
		std::uint32_t windowCount;
		std::uint32_t pixelBytes;
		std::uint32_t reserved;
		SurfaceRecord surfaces[MaximumWindows];
		std::uint8_t pixels[MaximumFramePixelBytes];
	};

	struct SharedState
	{
		std::uint32_t magic;
		std::uint32_t version;
		std::uint32_t structureBytes;
		std::uint32_t reserved;
		volatile std::int32_t lifecycle;
		volatile std::int32_t failureCode;
		volatile std::int32_t brokerProcessId;
		volatile std::int32_t commandWrite;
		volatile std::int32_t commandRead;
		volatile std::int32_t activeFrame;
		char failureMessage[MaximumFailureBytes];
		WindowState windows[MaximumWindows];
		Command commands[MaximumCommands];
		Frame frames[2];
	};
#pragma pack(pop)

	constexpr std::size_t SharedStateBytes = sizeof(SharedState);

	static_assert(sizeof(std::uint32_t) == 4u, "browser bridge requires 32-bit fixed-width fields");
	static_assert(sizeof(std::uint16_t) == 2u, "browser bridge requires UTF-16 code units");
	static_assert(std::is_standard_layout<Command>::value, "browser command must remain pointer-free POD");
	static_assert(std::is_standard_layout<WindowState>::value, "browser state must remain pointer-free POD");
	static_assert(std::is_standard_layout<Frame>::value, "browser frame must remain pointer-free POD");
	static_assert(std::is_standard_layout<SharedState>::value, "browser mapping must remain pointer-free POD");
	static_assert(sizeof(Command) == 36916u, "browser command layout changed; bump Version deliberately");
	static_assert(sizeof(WindowState) == 5192u, "browser state layout changed; bump Version deliberately");
	static_assert(sizeof(SurfaceRecord) == 32u, "browser surface layout changed; bump Version deliberately");
	static_assert(sizeof(Frame) == 50331920u, "browser frame layout changed; bump Version deliberately");
	static_assert(offsetof(SharedState, windows) == 552u, "browser window-state offset changed");
	static_assert(offsetof(SharedState, commands) == 42088u, "browser command offset changed");
	static_assert(offsetof(SharedState, frames) == 2404712u, "browser frame offset changed");
	static_assert(sizeof(SharedState) == 103068552u, "browser mapping layout changed; bump Version deliberately");
	static_assert(offsetof(SharedState, frames) % 4u == 0u, "browser frame mapping is misaligned");
	static_assert(SharedStateBytes < 128u * 1024u * 1024u, "browser bridge mapping grew unexpectedly");
}

#endif
