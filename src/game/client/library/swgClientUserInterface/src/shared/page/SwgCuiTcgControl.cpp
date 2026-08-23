// ======================================================================
//
// SwgCuiTcgControl.cpp
// copyright (c) 2008 Sony Online Entertainment LLC
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiTcgControl.h"

#include "libEverQuestTCG/libEverQuestTCG.h"

#include "clientGraphics/ShaderTemplateList.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/Texture.def"
#include "clientGraphics/Texture.h"
#include "clientGraphics/TextureList.h"
#include "clientUserInterface/CuiLayer.h"
#include "clientUserInterface/CuiLayer_TextureCanvas.h"

#include "UICanvas.h"
#include "UIImage.h"
#include "UIMessage.h"

#include <algorithm>
#include <limits>

// ======================================================================

namespace
{
	size_t const cs_tcgBytesPerPixel = 4;

	bool isValidSurfaceLayout(size_t width, size_t height, size_t pitch)
	{
		if (width == 0 || height == 0 || pitch == 0 || width > std::numeric_limits<size_t>::max() / cs_tcgBytesPerPixel)
			return false;

		size_t const rowBytes = width * cs_tcgBytesPerPixel;
		if (pitch < rowBytes)
			return false;

		return height == 1 || (height - 1) <= (std::numeric_limits<size_t>::max() - rowBytes) / pitch;
	}

	int mapMouseCoordinate(int coordinate, int controlExtent, unsigned surfaceExtent)
	{
		if (controlExtent <= 0 || surfaceExtent == 0 || surfaceExtent > static_cast<unsigned>(std::numeric_limits<int>::max()))
			return coordinate;

		// Preserve an out-of-control coordinate as out of bounds so drag/leave behavior remains intact.
		if (coordinate < 0)
			return -1;
		if (coordinate >= controlExtent)
			return static_cast<int>(surfaceExtent);

		unsigned long long const mapped =
			static_cast<unsigned long long>(static_cast<unsigned>(coordinate)) * surfaceExtent /
			static_cast<unsigned>(controlExtent);
		return static_cast<int>(mapped);
	}

#if defined(_WIN64)
	int addNativeMouseCoordinate(int origin, int localCoordinate)
	{
		long long const value = static_cast<long long>(origin) + static_cast<long long>(localCoordinate);
		if (value < static_cast<long long>(std::numeric_limits<int>::min()))
			return std::numeric_limits<int>::min();
		if (value > static_cast<long long>(std::numeric_limits<int>::max()))
			return std::numeric_limits<int>::max();
		return static_cast<int>(value);
	}
#endif
}

// ======================================================================

SwgCuiTcgControl::SwgCuiTcgControl()
: UIWidget()
, m_eqTcgWindow(0)
, m_image(0)
, m_texture(0)
, m_reportedFirstFrame(false)
, m_reportedInputDispatch(false)
, m_reportedInputMapping(false)
, m_horizontalSampleOffsets()
, m_horizontalMapSourceWidth(0)
, m_horizontalMapDestinationWidth(0)
{
}

// ----------------------------------------------------------------------

SwgCuiTcgControl::~SwgCuiTcgControl()
{
	if (m_texture)
		m_texture->release();

	m_eqTcgWindow = 0;
	m_image = 0;
	m_texture = 0;
}

// ----------------------------------------------------------------------

void SwgCuiTcgControl::alter(float deltaTime)
{
	UNREF(deltaTime);

	unsigned numWindows = libEverQuestTCG::getWindows(0, 0);

	if (m_eqTcgWindow && numWindows < 1)
	{
		setEqTcgWindow(0);
		libEverQuestTCG::release();
		return;
	}

	fetchTexture();
}

// ----------------------------------------------------------------------

void SwgCuiTcgControl::Render(UICanvas & canvas) const
{
	UNREF(canvas);

	if (!m_eqTcgWindow || !m_image || !m_texture)
	{
		DEBUG_REPORT_PRINT(!m_eqTcgWindow, ("SwgCuiTcgControl::Render() - m_eqTcgWindow is null\n"));
		DEBUG_REPORT_PRINT(!m_image, ("SwgCuiTcgControl::Render() - m_image is null\n"));
		DEBUG_REPORT_PRINT(!m_texture, ("SwgCuiTcgControl::Render() - m_texture is null\n"));
		return;
	}

	char * sourceBits = 0;
	unsigned sourceWidth = 0;
	unsigned sourceHeight = 0;
	unsigned sourceStride = 0;

	bool const result = m_eqTcgWindow->getWindowSurfaceData(reinterpret_cast<void **>(&sourceBits), &sourceWidth, &sourceHeight, &sourceStride);

	int const textureWidth = m_texture->getWidth();
	int const textureHeight = m_texture->getHeight();
	if (!result || !sourceBits || textureWidth <= 0 || textureHeight <= 0 ||
		!isValidSurfaceLayout(sourceWidth, sourceHeight, sourceStride))
		return;

	Texture::LockData lockData(TF_XRGB_8888, 0, 0, 0, textureWidth, textureHeight, true);
	m_texture->lock(lockData);

	char * const textureData = static_cast<char *>(lockData.getPixelData());
	if (!textureData)
	{
		m_texture->unlock(lockData);
		return;
	}

	int const destinationPitchValue = lockData.getPitch();
	if (destinationPitchValue <= 0 ||
		!isValidSurfaceLayout(static_cast<size_t>(textureWidth), static_cast<size_t>(textureHeight), static_cast<size_t>(destinationPitchValue)))
	{
		m_texture->unlock(lockData);
		return;
	}

	size_t const destinationPitch = static_cast<size_t>(destinationPitchValue);
	size_t const sourceWidthValue = static_cast<size_t>(sourceWidth);
	size_t const sourceHeightValue = static_cast<size_t>(sourceHeight);
	size_t const destinationWidth = static_cast<size_t>(textureWidth);
	size_t const destinationHeight = static_cast<size_t>(textureHeight);
	size_t const destinationVisibleBytes = destinationWidth * cs_tcgBytesPerPixel;

	if (sourceWidthValue == destinationWidth && sourceHeightValue == destinationHeight &&
		sourceStride == destinationVisibleBytes && destinationPitch == destinationVisibleBytes)
	{
		memcpy(textureData, sourceBits, destinationVisibleBytes * destinationHeight);
	}
	else if (sourceWidthValue == destinationWidth && sourceHeightValue == destinationHeight)
	{
		for (size_t row = 0; row < destinationHeight; ++row)
		{
			memcpy(textureData + row * destinationPitch,
				sourceBits + row * static_cast<size_t>(sourceStride), destinationVisibleBytes);
		}
	}
	else
	{
		// SWGTCG can retain its native surface size after the embedded control requests a resize.
		// Scale the complete surface into the control so visible pixels and forwarded input agree.
		// Cache the exact nearest-neighbor horizontal map, then advance source rows with a
		// quotient/remainder accumulator. This removes integer division and tiny memcpy calls
		// from the per-pixel hot loop while retaining bounds-checked byte access.
		if (!prepareHorizontalSampleMap(sourceWidthValue, destinationWidth))
		{
			m_texture->unlock(lockData);
			return;
		}

		size_t const sourceRowAdvance = sourceHeightValue / destinationHeight;
		size_t const sourceRowRemainder = sourceHeightValue % destinationHeight;
		size_t sourceY = 0;
		size_t sourceRowError = 0;
		size_t const * const horizontalSampleOffsets = &m_horizontalSampleOffsets.front();

		for (size_t destinationY = 0; destinationY < destinationHeight; ++destinationY)
		{
			unsigned char const * const sourceRow = reinterpret_cast<unsigned char const *>(
				sourceBits + sourceY * static_cast<size_t>(sourceStride));
			unsigned char * const destinationRow = reinterpret_cast<unsigned char *>(
				textureData + destinationY * destinationPitch);
			size_t destinationOffset = 0;
			for (size_t destinationX = 0; destinationX < destinationWidth; ++destinationX)
			{
				unsigned char const * const sourcePixel = sourceRow + horizontalSampleOffsets[destinationX];
				destinationRow[destinationOffset] = sourcePixel[0];
				destinationRow[destinationOffset + 1] = sourcePixel[1];
				destinationRow[destinationOffset + 2] = sourcePixel[2];
				destinationRow[destinationOffset + 3] = sourcePixel[3];
				destinationOffset += cs_tcgBytesPerPixel;
			}

			sourceY += sourceRowAdvance;
			sourceRowError += sourceRowRemainder;
			if (sourceRowError >= destinationHeight)
			{
				sourceRowError -= destinationHeight;
				++sourceY;
			}
		}
	}

	m_texture->unlock(lockData);

	CuiLayer::TextureCanvas const * constCanvas = safe_cast<CuiLayer::TextureCanvas const *>(m_image->GetCanvas());

	CuiLayer::TextureCanvas * textureCanvas = const_cast<CuiLayer::TextureCanvas *>(constCanvas);

	if (textureCanvas)
	{
		ShaderTemplate const * const shaderTemplate = ShaderTemplateList::fetch("shader/uicanvas_filtered.sht");

		NOT_NULL(shaderTemplate);

		if (!shaderTemplate)
			return;

		StaticShader * const newShader = safe_cast<StaticShader *>(NON_NULL(shaderTemplate->fetchModifiableShader()));

		if (!newShader)
		{
			shaderTemplate->release();
			return;
		}

		newShader->setTexture(TAG(M,A,I,N), *m_texture);

		textureCanvas->SetSize(UISize(GetWidth(), GetHeight()));

		if (textureCanvas->getShader() != newShader)
			textureCanvas->SetShader(newShader);

		textureCanvas->SetTextureName(m_texture->getName());

		canvas.BltFrom(textureCanvas, UIPoint::zero, UIPoint::zero, GetSize());

		shaderTemplate->release();

		if (!m_reportedFirstFrame)
		{
			m_reportedFirstFrame = true;
			REPORT_LOG(true, ("TCG integration: embedded-control-frame source=%ux%u stride=%u destination=%dx%d pitch=%d.\n",
				sourceWidth, sourceHeight, sourceStride, textureWidth, textureHeight, destinationPitchValue));
		}
	}
}

// ----------------------------------------------------------------------

UIBaseObject * SwgCuiTcgControl::Clone() const
{
	return new SwgCuiTcgControl;
}

// ----------------------------------------------------------------------

UIStyle * SwgCuiTcgControl::GetStyle() const
{
	return 0;
}

// ----------------------------------------------------------------------

void SwgCuiTcgControl::setImage(UIImage * image)
{
	m_image = image;

	if (m_image)
		m_image->SetVisible(false);
}

// ----------------------------------------------------------------------

void SwgCuiTcgControl::OnSizeChanged(const UISize &newSize, const UISize &oldSize)
{
	UIWidget::OnSizeChanged(newSize, oldSize);

	// The retail TCG uses a fixed native layout. Resizing its hidden Win32 window in the
	// x64 compatibility host reflows and clips controls instead of scaling them. Keep its
	// native surface and scale it in Render(); ProcessMessage() applies the inverse map.
#if !defined(_WIN64)
	if (m_eqTcgWindow && GetWidth() > 0 && GetHeight() > 0)
	{
		m_eqTcgWindow->setSize(static_cast<unsigned>(GetWidth()), static_cast<unsigned>(GetHeight()));
	}
#endif

	if (m_image)
		m_image->SetSize(UIPoint(GetWidth(), GetHeight()));

	if (m_texture)
	{
		m_texture->release();
		m_texture = NULL;
	}
	fetchTexture();
}

// ----------------------------------------------------------------------

void SwgCuiTcgControl::initializeEqTcgWindow()
{
	if (m_eqTcgWindow)
		return;

	// use the first window
	unsigned numWindows = libEverQuestTCG::getWindows(0, 0);

	if (numWindows > 0)
	{
		libEverQuestTCG::Window * firstWindow = 0;
		libEverQuestTCG::getWindows(&firstWindow, 1);

		if (firstWindow)
		{
			setEqTcgWindow(firstWindow);
#if !defined(_WIN64)
			if (GetSize().x > 0 && GetSize().y > 0)
				m_eqTcgWindow->setSize(static_cast<unsigned>(GetSize().x), static_cast<unsigned>(GetSize().y));
#endif

			alter(0);
		}
	}
}

// ----------------------------------------------------------------------

bool SwgCuiTcgControl::ProcessMessage(const UIMessage & msg)
{
	if (m_eqTcgWindow)
	{
		if (IsSelected())
			m_eqTcgWindow->setFocus(true);

		unsigned int flags = 0;

		if (msg.Modifiers.isAltDown())
			flags |= libEverQuestTCG::Window::ALT;
		if (msg.Modifiers.isControlDown())
			flags |= libEverQuestTCG::Window::CONTROL;
		if (msg.Modifiers.isShiftDown())
			flags |= libEverQuestTCG::Window::SHIFT;
		if (msg.Modifiers.LeftMouseDown)
			flags |= libEverQuestTCG::Window::LeftButton;
		if (msg.Modifiers.RightMouseDown)
			flags |= libEverQuestTCG::Window::RightButton;
		if (msg.Modifiers.MiddleMouseDown)
			flags |= libEverQuestTCG::Window::MiddleButton;

		int x = msg.MouseCoords.x;
		int y = msg.MouseCoords.y;
		UIPoint gpos = GetWorldLocation();
#if defined(_WIN64)
		bool mappedToNativeSurface = false;
#endif

		bool isMouseMessage = false;
		switch (msg.Type)
		{
		case UIMessage::MouseMove:
		case UIMessage::LeftMouseDown:
		case UIMessage::LeftMouseUp:
		case UIMessage::MiddleMouseDown:
		case UIMessage::MiddleMouseUp:
		case UIMessage::RightMouseDown:
		case UIMessage::RightMouseUp:
		case UIMessage::MouseWheel:
		case UIMessage::LeftMouseDoubleClick:
		case UIMessage::RightMouseDoubleClick:
		case UIMessage::MiddleMouseDoubleClick:
			isMouseMessage = true;
			break;
		default:
			break;
		}

		void * surfaceBits = 0;
		unsigned surfaceWidth = 0;
		unsigned surfaceHeight = 0;
		unsigned surfaceStride = 0;
		if (isMouseMessage &&
			m_eqTcgWindow->getWindowSurfaceData(&surfaceBits, &surfaceWidth, &surfaceHeight, &surfaceStride) &&
			surfaceBits && isValidSurfaceLayout(surfaceWidth, surfaceHeight, surfaceStride))
		{
			int const controlWidth = GetWidth();
			int const controlHeight = GetHeight();
			x = mapMouseCoordinate(x, controlWidth, surfaceWidth);
			y = mapMouseCoordinate(y, controlHeight, surfaceHeight);
#if defined(_WIN64)
			mappedToNativeSurface = true;
#endif

			if (!m_reportedInputMapping)
			{
				m_reportedInputMapping = true;
				REPORT_LOG(true, ("TCG integration: embedded-control-input-map control=%dx%d surface=%ux%u.\n",
					controlWidth, controlHeight, surfaceWidth, surfaceHeight));
			}
		}

		int gx = x + gpos.x;
		int gy = y + gpos.y;
#if defined(_WIN64)
		if (isMouseMessage && mappedToNativeSurface)
		{
			int nativeX = 0;
			int nativeY = 0;
			unsigned nativeWidth = 0;
			unsigned nativeHeight = 0;
			m_eqTcgWindow->getRect(nativeX, nativeY, nativeWidth, nativeHeight);
			UNREF(nativeWidth);
			UNREF(nativeHeight);
			gx = addNativeMouseCoordinate(nativeX, x);
			gy = addNativeMouseCoordinate(nativeY, y);
		}
#endif

#ifdef _DEBUG
		bool const enablePrintLog = false;
#endif

		DEBUG_REPORT_PRINT(enablePrintLog, ("mouse.x = %d, mouse.y = %d\n", x, y));
		DEBUG_REPORT_PRINT(enablePrintLog, ("gpos.x = %d, gpos.y = %d\n", gpos.x, gpos.y));
		DEBUG_REPORT_PRINT(enablePrintLog, ("gx = %d, gy = %d\n", gx, gy));

		switch (msg.Type)
		{
		case UIMessage::MouseMove:
			{
				if (!m_reportedInputDispatch)
				{
					m_reportedInputDispatch = true;
					REPORT_LOG(true, ("TCG integration: embedded-control-input-dispatched.\n"));
				}
				DEBUG_REPORT_LOG(enablePrintLog, ("%s got a Mouse Move Down Message. Params: X = %i Y= %i gx = %i gy = %i\n", GetName().c_str(), x, y, gx, gy));
				m_eqTcgWindow->onMouseMove(x, y, gx, gy, flags);
				return true;
			}
			break;
		case UIMessage::LeftMouseDown:
			{
				DEBUG_REPORT_LOG(enablePrintLog, ("%s got a Left Mouse Down Message. Params: X = %i Y= %i gx = %i gy = %i\n", GetName().c_str(), x, y, gx, gy));
				m_eqTcgWindow->onLeftMouseDown(x, y, gx, gy, flags);
				return true;
			}
			break;
		case UIMessage::LeftMouseUp:
			{
				m_eqTcgWindow->onLeftMouseUp(x, y, gx, gy, flags);
				return true;
			}
			break;
		case UIMessage::MiddleMouseDown:
			{
				m_eqTcgWindow->onMiddleMouseDown(x, y, gx, gy, flags);
				return true;
			}
			break;
		case UIMessage::MiddleMouseUp:
			{
				m_eqTcgWindow->onMiddleMouseUp(x, y, gx, gy, flags);
				return true;
			}
			break;
		case UIMessage::RightMouseDown:
			{
				DEBUG_REPORT_LOG(enablePrintLog, ("%s got a Right Mouse Down Message. Params: X = %i Y= %i gx = %i gy = %i\n", GetName().c_str(), x, y, gx, gy));
				m_eqTcgWindow->onRightMouseDown(x, y, gx, gy, flags);
				return true;
			}
			break;
		case UIMessage::RightMouseUp:
			{
				m_eqTcgWindow->onRightMouseUp(x, y, gx, gy, flags);
				return true;
			}
			break;
		case UIMessage::MouseWheel:
			{
				m_eqTcgWindow->onMouseWheel(x, y, gx, gy, msg.Data * 16, flags);
				return true;
			}
			break;
		case UIMessage::LeftMouseDoubleClick:
			{
				m_eqTcgWindow->onLeftMouseDoubleClick(x, y, gx, gy, flags);
				return true;
			}
			break;
		case UIMessage::RightMouseDoubleClick:
			{
				m_eqTcgWindow->onRightMouseDoubleClick(x, y, gx, gy, flags);
				return true;
			}
			break;
		case UIMessage::MiddleMouseDoubleClick:
			{
				m_eqTcgWindow->onMiddleMouseDoubleClick(x, y, gx, gy, flags);
				return true;
			}
			break;
		case UIMessage::KeyUp:
		case UIMessage::KeyDown:
		case UIMessage::KeyRepeat:
		case UIMessage::Character:
			{
				int key = msg.Keystroke;

				if (key == UIMessage::Escape)
					key = libEverQuestTCG::Key_Escape;
				else if (key == UIMessage::Tab)
					key = libEverQuestTCG::Key_Tab;
				else if (key == UIMessage::BackSpace)
					key = libEverQuestTCG::Key_Backspace;
				else if (key == UIMessage::Enter)
					key = libEverQuestTCG::Key_Enter;
				else if (key == UIMessage::Insert)
					key = libEverQuestTCG::Key_Insert;     
				else if (key == UIMessage::Delete)
					key = libEverQuestTCG::Key_Delete;
				else if (key == UIMessage::BackSpace)
					key = libEverQuestTCG::Key_Clear;      
				else if (key == UIMessage::Home)
					key = libEverQuestTCG::Key_Home;       
				else if (key == UIMessage::End)
					key = libEverQuestTCG::Key_End;        
				else if (key == UIMessage::LeftArrow)
					key = libEverQuestTCG::Key_Left;       
				else if (key == UIMessage::UpArrow)
					key = libEverQuestTCG::Key_Up;         
				else if (key == UIMessage::RightArrow)
					key = libEverQuestTCG::Key_Right;      
				else if (key == UIMessage::DownArrow)
					key = libEverQuestTCG::Key_Down;       
				else if (key == UIMessage::PageUp)
					key = libEverQuestTCG::Key_PageUp;     
				else if (key == UIMessage::PageDown)
					key = libEverQuestTCG::Key_PageDown;

				if (msg.Type == UIMessage::KeyUp)
					m_eqTcgWindow->onKeyUp(key, flags, 0, 0, 0);
				else
				{
					bool const accepted = m_eqTcgWindow->onKeyDown(key, flags, 0, 0, 0);
					UNREF(accepted);

					DEBUG_REPORT_LOG(enablePrintLog, ("onKeyDown - [%s]\n", accepted ? "accepted" : "not accepted"));
				}

				return true;
			}
		default:
			return UIWidget::ProcessMessage(msg);
		}

		return true;
	}

	return UIWidget::ProcessMessage(msg);
}

// ----------------------------------------------------------------------

bool SwgCuiTcgControl::dispatchIntegrationTestClick(
	unsigned normalizedX,
	unsigned normalizedWidth,
	unsigned normalizedY,
	unsigned normalizedHeight)
{
	int const width = GetWidth();
	int const height = GetHeight();
	if (!m_eqTcgWindow || width <= 0 || height <= 0 ||
		normalizedWidth == 0 || normalizedHeight == 0 ||
		normalizedX >= normalizedWidth || normalizedY >= normalizedHeight)
	{
		return false;
	}

	unsigned long long const scaledXValue =
		static_cast<unsigned long long>(static_cast<unsigned>(width)) * normalizedX / normalizedWidth;
	unsigned long long const scaledYValue =
		static_cast<unsigned long long>(static_cast<unsigned>(height)) * normalizedY / normalizedHeight;
	if (scaledXValue >= static_cast<unsigned long long>(width) ||
		scaledYValue >= static_cast<unsigned long long>(height))
	{
		return false;
	}

	UIPoint const clickPoint(static_cast<long>(scaledXValue), static_cast<long>(scaledYValue));
	UIMessage mouseMove;
	mouseMove.Type = UIMessage::MouseMove;
	mouseMove.MouseCoords = clickPoint;
	if (!ProcessMessage(mouseMove))
		return false;

	UIMessage leftDown;
	leftDown.Type = UIMessage::LeftMouseDown;
	leftDown.MouseCoords = clickPoint;
	leftDown.Modifiers.LeftMouseDown = true;
	if (!ProcessMessage(leftDown))
		return false;

	UIMessage leftUp;
	leftUp.Type = UIMessage::LeftMouseUp;
	leftUp.MouseCoords = clickPoint;
	return ProcessMessage(leftUp);
}

// ----------------------------------------------------------------------

void SwgCuiTcgControl::SetSelected(const bool selected)
{		
	if (IsSelected() != selected && m_eqTcgWindow && m_eqTcgWindow->canGetFocus())
	{
		UIWidget::SetSelected(selected);
		m_eqTcgWindow->setFocus(selected);
	}
}

// ----------------------------------------------------------------------

void SwgCuiTcgControl::setEqTcgWindow(libEverQuestTCG::Window * eqTcgWindow)
{
	if (m_eqTcgWindow == eqTcgWindow)
		return;

	m_eqTcgWindow = eqTcgWindow;
	m_reportedFirstFrame = false;
	m_reportedInputDispatch = false;
	m_reportedInputMapping = false;

	if (m_eqTcgWindow)
		REPORT_LOG(true, ("TCG integration: embedded-control-bound focus-capable=%d.\n", m_eqTcgWindow->canGetFocus() ? 1 : 0));
}

// ----------------------------------------------------------------------

bool SwgCuiTcgControl::prepareHorizontalSampleMap(size_t sourceWidth, size_t destinationWidth) const
{
	if (sourceWidth == 0 || destinationWidth == 0 ||
		sourceWidth > std::numeric_limits<size_t>::max() / cs_tcgBytesPerPixel)
	{
		return false;
	}

	if (m_horizontalMapSourceWidth == sourceWidth &&
		m_horizontalMapDestinationWidth == destinationWidth &&
		m_horizontalSampleOffsets.size() == destinationWidth)
	{
		return true;
	}

	m_horizontalSampleOffsets.resize(destinationWidth);
	size_t const sourceAdvance = sourceWidth / destinationWidth;
	size_t const sourceRemainder = sourceWidth % destinationWidth;
	size_t sourceX = 0;
	size_t sourceError = 0;
	for (size_t destinationX = 0; destinationX < destinationWidth; ++destinationX)
	{
		if (sourceX >= sourceWidth)
		{
			m_horizontalSampleOffsets.clear();
			m_horizontalMapSourceWidth = 0;
			m_horizontalMapDestinationWidth = 0;
			return false;
		}

		m_horizontalSampleOffsets[destinationX] = sourceX * cs_tcgBytesPerPixel;
		sourceX += sourceAdvance;
		sourceError += sourceRemainder;
		if (sourceError >= destinationWidth)
		{
			sourceError -= destinationWidth;
			++sourceX;
		}
	}

	m_horizontalMapSourceWidth = sourceWidth;
	m_horizontalMapDestinationWidth = destinationWidth;
	return true;
}

// ----------------------------------------------------------------------

void SwgCuiTcgControl::fetchTexture()
{
	if (!m_texture && GetWidth() > 0 && GetHeight() > 0)
	{
		TextureFormat const runtimeFormats[] = {TF_XRGB_8888};
		int const numberOfRuntimeFormats = sizeof(runtimeFormats) / sizeof(runtimeFormats[0]);

		m_texture = const_cast<Texture *>(TextureList::fetch(static_cast<int>(0), GetWidth(), GetHeight(), 1, runtimeFormats, numberOfRuntimeFormats));
	}
}

// ----------------------------------------------------------------------

void SwgCuiTcgControl::OnLocationChanged(const UIPoint &newLocation, const UIPoint &oldLocation)
{
	UIWidget::OnLocationChanged(newLocation, oldLocation);
}

// ======================================================================
