#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "libMozilla.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#if !defined(_WIN64)
#error BrowserCompatibilitySmokeTest exercises the x64 libMozilla proxy.
#endif

namespace
{
	std::string wideToAnsi(wchar_t const * value)
	{
		int const required = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, value, -1,
			nullptr, 0, nullptr, nullptr);
		if (required <= 1)
			return std::string();
		std::string result(static_cast<std::size_t>(required), '\0');
		if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, value, -1, result.data(),
			required, nullptr, nullptr) != required)
		{
			return std::string();
		}
		result.pop_back();
		return result;
	}

	class Callback final : public libMozilla::ICallback
	{
	public:
		explicit Callback(std::string expectedUri)
		: expectedUri(std::move(expectedUri))
		{
		}

		void onURIChanged(libMozilla::Window * window) override
		{
			char const * const uri = window ? window->getURI() : nullptr;
			sawExpectedUri = uri && expectedUri == uri;
		}
		void onProgressChanged(libMozilla::Window *) override {}
		void onStatusChanged(libMozilla::Window *) override {}
		bool doValidateURI(libMozilla::Window *, char const *) override { return true; }

		std::string expectedUri;
		bool sawExpectedUri = false;
	};

	class Blitter final : public libMozilla::IBlitter
	{
	public:
		void operator()(void * bits, unsigned width, unsigned height, unsigned stride,
			unsigned bytesPerRow) override
		{
			if (!bits || width == 0u || height == 0u || stride < width * 4u || bytesPerRow < width * 4u)
				return;
			std::uint8_t const * const pixels = static_cast<std::uint8_t const *>(bits);
			std::size_t const byteCount = static_cast<std::size_t>(stride) * height;
			for (std::size_t index = 0u; index < byteCount; ++index)
			{
				// Ignore the forced alpha byte; require nonzero rendered color data.
				if ((index % 4u) != 3u && pixels[index] != 0u)
				{
					hasNonzeroSurface = true;
					capturedWidth = width;
					capturedHeight = height;
					return;
				}
			}
		}

		bool hasNonzeroSurface = false;
		unsigned capturedWidth = 0u;
		unsigned capturedHeight = 0u;
	};
}

int wmain(int argumentCount, wchar_t * arguments[])
{
	if (argumentCount != 3)
	{
		std::fprintf(stderr, "usage: BrowserCompatibilitySmokeTest <runtime\\mozilla-broker> <loopback-url>\n");
		return 2;
	}
	std::string const runtime = wideToAnsi(arguments[1]);
	if (runtime.empty() || !libMozilla::init(nullptr, runtime.c_str()))
	{
		std::fprintf(stderr, "x64 browser proxy initialization failed\n");
		return 3;
	}
	libMozilla::Window * const window = libMozilla::createWindow(320u, 200u);
	if (!window)
	{
		std::fprintf(stderr, "x64 browser proxy window creation failed\n");
		libMozilla::release();
		return 4;
	}
	std::string const expectedUri = wideToAnsi(arguments[2]);
	Callback callback(expectedUri);
	Blitter blitter;
	window->setCallback(&callback);
	window->navigateTo(arguments[2], nullptr, 0u);
	ULONGLONG const deadline = GetTickCount64() + 15000u;
	while ((!blitter.hasNonzeroSurface || !callback.sawExpectedUri) && GetTickCount64() < deadline)
	{
		libMozilla::update();
		(void)window->render(&blitter);
		Sleep(10u);
	}
	libMozilla::destroyWindow(window);
	libMozilla::release();
	if (!blitter.hasNonzeroSurface || !callback.sawExpectedUri)
	{
		std::fprintf(stderr, "loopback URI and nonzero in-game BGRA surface were not both observed\n");
		return 5;
	}
	std::printf("nonzero BGRA surface: %ux%u\n", blitter.capturedWidth, blitter.capturedHeight);
	return 0;
}
