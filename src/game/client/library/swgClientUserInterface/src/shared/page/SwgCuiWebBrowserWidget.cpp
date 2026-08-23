// ======================================================================
//
// SwgCuiWebBrowserWidget.cpp
// copyright (c) 2008 Sony Online Entertainment LLC
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiWebBrowserWidget.h"


#include "clientGraphics/Graphics.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/ShaderTemplateList.h"
#include "clientGraphics/DynamicVertexBuffer.h"
#include "clientGraphics/Texture.h"
#include "clientGraphics/Texture.def"
#include "clientGraphics/TextureList.h"
#include "clientUserInterface/CuiLayer_TextureCanvas.h"

#include "sharedFoundation/Os.h"

#include "swgClientUserInterface/SwgCuiChatWindow.h"
#include "swgClientUserInterface/SwgCuiHud.h"
#include "swgClientUserInterface/SwgCuiHudFactory.h"
#include "swgClientUserInterface/SwgCuiWebBrowserWindow.h"

#if DEBUG == 0
#include "libMozilla/libMozilla.h"
#endif

#include "UnicodeUtils.h"

#include "UICanvas.h"
#include "UIImage.h"
#include "UIPage.h"
#include "UIMessage.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

// ======================================================================

#if DEBUG == 0
namespace browserNamespace
{
	std::string s_homePage = "beta.stellabellum.net";
}

using namespace browserNamespace;

class UIMozillaCallbacks : public libMozilla::ICallback
{
	SwgCuiWebBrowserWidget *m_MozillaWidget;

public:

	UIMozillaCallbacks( SwgCuiWebBrowserWidget *mozillaWidget ) :
	m_MozillaWidget( mozillaWidget )
	{

	}

	virtual void onURIChanged( libMozilla::Window * )
	{
		m_MozillaWidget->updateURI();
	}
	virtual void onProgressChanged( libMozilla::Window * )
	{
		m_MozillaWidget->updateProgress();
	}
	virtual void onStatusChanged( libMozilla::Window * )
	{
		m_MozillaWidget->updateStatus();
	}
	virtual bool doValidateURI( libMozilla::Window *, const char *pURI )
	{
		return m_MozillaWidget->validateURI( pURI );
	}
};

class UIMozillaCanvasBlitter : public libMozilla::IBlitter
{
	Texture *m_pTexture;
	bool m_inspectNonzeroPixels;
	bool m_hasNonzeroPixels;
	bool m_blitComplete;

public:

	UIMozillaCanvasBlitter(Texture* texture, bool inspectNonzeroPixels)
		: m_pTexture( texture )
		, m_inspectNonzeroPixels(inspectNonzeroPixels)
		, m_hasNonzeroPixels(false)
		, m_blitComplete(false)
	{
	}

	void operator()( void *pSource, unsigned uSourceWidth, unsigned uSourceHeight, unsigned uSourceStride, unsigned uSourceBytesPerRow )
	{
		m_blitComplete = false;

		if (!m_pTexture || !pSource || uSourceWidth == 0u || uSourceHeight == 0u ||
			uSourceStride == 0u || uSourceBytesPerRow == 0u)
			return;

		int const destinationWidth = m_pTexture->getWidth();
		int const destinationHeight = m_pTexture->getHeight();
		if (destinationWidth <= 0 || destinationHeight <= 0)
			return;

		std::size_t const maximumSize = (std::numeric_limits<std::size_t>::max)();
		std::size_t const sourceWidth = static_cast<std::size_t>(uSourceWidth);
		std::size_t const sourceHeight = static_cast<std::size_t>(uSourceHeight);
		std::size_t const sourceStride = static_cast<std::size_t>(uSourceStride);
		std::size_t const sourceBytesPerRow = static_cast<std::size_t>(uSourceBytesPerRow);
		if (sourceWidth > maximumSize / 4u)
			return;

		std::size_t const requiredFourByteRow = sourceWidth * 4u;
		std::size_t const requiredThreeByteRow = sourceWidth * 3u;
		std::size_t const readableSourceRow = (std::min)(sourceStride, sourceBytesPerRow);
		std::size_t sourcePixelBytes = 0u;
		if (readableSourceRow >= requiredFourByteRow)
			sourcePixelBytes = 4u;
		else if (readableSourceRow >= requiredThreeByteRow)
			sourcePixelBytes = 3u;
		else
			return;

		std::size_t const requiredSourceRow = sourceWidth * sourcePixelBytes;
		if ((sourceHeight - 1u) > maximumSize / sourceStride)
			return;
		std::size_t const lastSourceRowOffset = (sourceHeight - 1u) * sourceStride;
		if (requiredSourceRow > maximumSize - lastSourceRowOffset)
			return;

		std::size_t const destinationWidthSize = static_cast<std::size_t>(destinationWidth);
		std::size_t const destinationHeightSize = static_cast<std::size_t>(destinationHeight);
		if (destinationWidthSize > maximumSize / 4u)
			return;
		std::size_t const requiredDestinationRow = destinationWidthSize * 4u;

		Texture::LockData lockData(TF_XRGB_8888, 0, 0, 0, destinationWidth, destinationHeight, true);
		m_pTexture->lock(lockData);

		unsigned char * const textureData = static_cast<unsigned char *>(lockData.getPixelData());
		if (!textureData)
		{
			m_pTexture->unlock(lockData);
			return;
		}

		int const destinationPitchValue = lockData.getPitch();
		if (destinationPitchValue <= 0)
		{
			m_pTexture->unlock(lockData);
			return;
		}

		std::size_t const destinationPitch = static_cast<std::size_t>(destinationPitchValue);
		if (destinationPitch < requiredDestinationRow ||
			(destinationHeightSize - 1u) > maximumSize / destinationPitch)
		{
			m_pTexture->unlock(lockData);
			return;
		}
		std::size_t const lastDestinationRowOffset = (destinationHeightSize - 1u) * destinationPitch;
		if (requiredDestinationRow > maximumSize - lastDestinationRowOffset)
		{
			m_pTexture->unlock(lockData);
			return;
		}

		unsigned char const * const source = static_cast<unsigned char const *>(pSource);
		if (sourceWidth == destinationWidthSize && sourceHeight == destinationHeightSize)
		{
			for (std::size_t row = 0u; row < destinationHeightSize; ++row)
			{
				unsigned char const * const sourceRow = source + row * sourceStride;
				unsigned char * const destinationRow = textureData + row * destinationPitch;
				if (sourcePixelBytes == 4u)
				{
					std::memcpy(destinationRow, sourceRow, requiredDestinationRow);
				}
				else
				{
					for (std::size_t column = 0u; column < destinationWidthSize; ++column)
					{
						unsigned char const * const sourcePixel = sourceRow + column * 3u;
						unsigned char * const destinationPixel = destinationRow + column * 4u;
						destinationPixel[0] = sourcePixel[0];
						destinationPixel[1] = sourcePixel[1];
						destinationPixel[2] = sourcePixel[2];
						destinationPixel[3] = 0xffu;
					}
				}

				if (m_inspectNonzeroPixels && !m_hasNonzeroPixels)
				{
					for (std::size_t column = 0u; column < destinationWidthSize; ++column)
					{
						unsigned char const * const sourcePixel = sourceRow + column * sourcePixelBytes;
						if (sourcePixel[0] != 0u || sourcePixel[1] != 0u || sourcePixel[2] != 0u)
						{
							m_hasNonzeroPixels = true;
							break;
						}
					}
				}
			}

			m_blitComplete = true;
			m_pTexture->unlock(lockData);
			return;
		}

		for (std::size_t destinationY = 0u; destinationY < destinationHeightSize; ++destinationY)
		{
			std::size_t const sourceY = static_cast<std::size_t>(
				(static_cast<std::uint64_t>(destinationY) * uSourceHeight) /
				static_cast<std::uint64_t>(destinationHeightSize));
			unsigned char const * const sourceRow = source + sourceY * sourceStride;
			unsigned char * const destinationRow = textureData + destinationY * destinationPitch;

			for (std::size_t destinationX = 0u; destinationX < destinationWidthSize; ++destinationX)
			{
				std::size_t const sourceX = static_cast<std::size_t>(
					(static_cast<std::uint64_t>(destinationX) * uSourceWidth) /
					static_cast<std::uint64_t>(destinationWidthSize));
				unsigned char const * const sourcePixel = sourceRow + sourceX * sourcePixelBytes;
				unsigned char * const destinationPixel = destinationRow + destinationX * 4u;
				destinationPixel[0] = sourcePixel[0];
				destinationPixel[1] = sourcePixel[1];
				destinationPixel[2] = sourcePixel[2];
				destinationPixel[3] = 0xffu;

				if (m_inspectNonzeroPixels && !m_hasNonzeroPixels &&
					(sourcePixel[0] != 0u || sourcePixel[1] != 0u || sourcePixel[2] != 0u))
				{
					m_hasNonzeroPixels = true;
				}
			}
		}

		m_blitComplete = true;
		m_pTexture->unlock(lockData);
	}

	bool hasNonzeroPixels() const
	{
		return m_hasNonzeroPixels;
	}

	bool isBlitComplete() const
	{
		return m_blitComplete;
	}

};

SwgCuiWebBrowserWidget::SwgCuiWebBrowserWidget()
: UIWidget(),
m_MozillaWindow(NULL),
m_Callbacks(NULL),
m_Texture(NULL),
m_Canvas(NULL),
m_Image(NULL),
m_Text(NULL),
m_Shader (NULL),
m_caratBlink (0.5f),
m_drawCarat (false),
m_integrationProbeNonce(),
m_reportedIntegrationProxyWindow(false),
m_reportedIntegrationFirstNonzeroFrame(false)
{


	//shaderTemplate->release();

}

// ----------------------------------------------------------------------

SwgCuiWebBrowserWidget::~SwgCuiWebBrowserWidget()
{
	if(m_MozillaWindow)
	{
		m_MozillaWindow->setCallback(0);
		libMozilla::destroyWindow(m_MozillaWindow);
		m_MozillaWindow = NULL;
	}

	if(m_Callbacks)
	{
		delete m_Callbacks;
		m_Callbacks = NULL;
	}
	m_integrationProbeNonce.clear();
}

void SwgCuiWebBrowserWidget::alter(float deltaTime)
{
	if(!m_Texture)
	{
		const TextureFormat  runtimeFormats[] = { TF_XRGB_8888};
		const int numberOfRuntimeFormats = sizeof(runtimeFormats) / sizeof(runtimeFormats[0]);

		m_Texture = const_cast<Texture*>(TextureList::fetch(static_cast<int>(0), GetWidth(), GetHeight(), 1, runtimeFormats, numberOfRuntimeFormats));
	}

	if(!m_MozillaWindow)
		m_MozillaWindow = getMozillaWindow();

	if(m_caratBlink.updateZero(deltaTime))
	{
		m_drawCarat = !m_drawCarat;
	}
}
// ----------------------------------------------------------------------

void SwgCuiWebBrowserWidget::Render(UICanvas & canvas) const
{
	UNREF(canvas);
	
	UICanvas * sourceCanvas = &canvas;

	if (sourceCanvas && m_Texture)
	{
		const CuiLayer::TextureCanvas* constCanvas = safe_cast< const CuiLayer::TextureCanvas *>(m_Image->GetCanvas());

		CuiLayer::TextureCanvas * textureCanvas = const_cast<CuiLayer::TextureCanvas*>(constCanvas);

		bool const inspectNonzeroPixels = !m_integrationProbeNonce.empty() && !m_reportedIntegrationFirstNonzeroFrame;
		UIMozillaCanvasBlitter blit(m_Texture, inspectNonzeroPixels);

		if(m_MozillaWindow && textureCanvas)
		{
			bool const renderComplete = m_MozillaWindow->render(&blit) && blit.isBlitComplete();
			
			if(renderComplete)
			{
				if (inspectNonzeroPixels && blit.hasNonzeroPixels())
				{
					m_reportedIntegrationFirstNonzeroFrame = true;
					REPORT_LOG(true, ("TCG integration: browser-probe-first-nonzero-frame nonce=[%s] pid=%lu width=%d height=%d.\n",
						m_integrationProbeNonce.c_str(),
						static_cast<unsigned long>(Os::getProcessId()),
						m_Texture->getWidth(),
						m_Texture->getHeight()));
					m_integrationProbeNonce.clear();
				}

				const ShaderTemplate * const shaderTemplate = ShaderTemplateList::fetch("shader/uicanvas_filtered.sht");

				NOT_NULL(shaderTemplate);

				StaticShader * const newShader = safe_cast<StaticShader *>(NON_NULL (shaderTemplate->fetchModifiableShader ()));

				newShader->setTexture(TAG(M,A,I,N), *m_Texture);

				textureCanvas->SetSize(UISize(GetWidth(), GetHeight()));

				if(textureCanvas->getShader() != newShader)
					textureCanvas->SetShader(newShader);

				textureCanvas->SetTextureName(m_Texture->getName());

				canvas.BltFrom(textureCanvas, UIPoint::zero, UIPoint::zero, GetSize());

				if( IsSelected() && m_drawCarat)
				{
					int x, y, width, height;
					if( m_MozillaWindow->getCaret( x, y, width, height ) )
					{
						canvas.ClearTo( UIColor::black, UIRect( x, y, x + width, y + height ) );
					}
				}

				shaderTemplate->release();
			}
			else
			{
				canvas.BltFrom(0, UIPoint::zero, UIPoint::zero, GetSize());
			}

		}

	}

}

// ----------------------------------------------------------------------

UIBaseObject * SwgCuiWebBrowserWidget::Clone() const
{
	return new SwgCuiWebBrowserWidget;
}

// ----------------------------------------------------------------------

UIStyle * SwgCuiWebBrowserWidget::GetStyle() const
{
	return 0;
}

libMozilla::Window* SwgCuiWebBrowserWidget::getMozillaWindow()
{
	if(!m_MozillaWindow)
	{
		m_MozillaWindow = libMozilla::createWindow(GetWidth(), GetHeight());
		
		if(m_MozillaWindow)
		{
			if (!m_integrationProbeNonce.empty() && !m_reportedIntegrationProxyWindow)
			{
				m_reportedIntegrationProxyWindow = true;
				REPORT_LOG(true, ("TCG integration: browser-probe-proxy-window-created nonce=[%s] pid=%lu width=%d height=%d.\n",
					m_integrationProbeNonce.c_str(),
					static_cast<unsigned long>(Os::getProcessId()),
					GetWidth(),
					GetHeight()));
			}

			if(!m_Callbacks)
				m_Callbacks = new UIMozillaCallbacks(this);

			m_MozillaWindow->setCallback(m_Callbacks);

			if (m_integrationProbeNonce.empty())
			{
				Unicode::String uri16 = Unicode::narrowToWide(s_homePage);

				m_MozillaWindow->navigateTo(
					reinterpret_cast<const wchar_t *>(uri16.c_str()),
					nullptr,
					0);
			}
		}
	}

	return m_MozillaWindow;
}

void SwgCuiWebBrowserWidget::updateProgress()
{

}

void SwgCuiWebBrowserWidget::updateStatus()
{

}

void SwgCuiWebBrowserWidget::updateURI()
{
	if(m_Text && m_MozillaWindow)
	{
		m_Text->SetLocalText(Unicode::narrowToWide(m_MozillaWindow->getURI()));
	}
}

bool SwgCuiWebBrowserWidget::validateURI(const char* /*uri*/)
{
	return true;
}

void SwgCuiWebBrowserWidget::setUIImage(UIImage* image)
{
	m_Image = image;
	image->SetVisible(false);
}

void SwgCuiWebBrowserWidget::debugOutput()
{
	if(!m_MozillaWindow)
		return;

	DEBUG_REPORT_LOG(true, ("Mozilla Browser Debug:\n\n"));
	bool loading;
	float progress;
	progress = m_MozillaWindow->getProgress(loading);

	DEBUG_REPORT_LOG(true, ("Current URI: %s\nLoading: %d\nProgress: %-3.1f\n", m_MozillaWindow->getURI(), static_cast<int>(loading), progress));
}

void SwgCuiWebBrowserWidget::setURL( std::string url, const char * postData, int postDataLength)
{
	if(!m_MozillaWindow)
		return;

	Unicode::String uri16 = Unicode::narrowToWide(url);

	m_MozillaWindow->navigateTo(
		reinterpret_cast<const wchar_t *>(uri16.c_str()),
		postData,
		postDataLength);
}

void SwgCuiWebBrowserWidget::OnSizeChanged(const UISize &newSize, const UISize &oldSize)
{
	UIWidget::OnSizeChanged(newSize, oldSize);

	if(m_MozillaWindow)
		m_MozillaWindow->setSize(GetWidth(), GetHeight());

	if(m_Image)
		m_Image->SetSize(UIPoint(GetWidth(), GetHeight()));

	if(m_Texture)
	{
		m_Texture->release();
		m_Texture = NULL;
	}

	if(!m_Texture)
	{
		const TextureFormat  runtimeFormats[] = { TF_XRGB_8888};
		const int numberOfRuntimeFormats = sizeof(runtimeFormats) / sizeof(runtimeFormats[0]);

		m_Texture = const_cast<Texture*>(TextureList::fetch(static_cast<int>(0), GetWidth(), GetHeight(), 1, runtimeFormats, numberOfRuntimeFormats));
	}
}

bool SwgCuiWebBrowserWidget::ProcessMessage(const UIMessage & msg)
{
	if (UIWidget::ProcessMessage( msg ))
		return true;

	if(m_MozillaWindow)
	{
		unsigned int Flags = 0;

		if(msg.Modifiers.isAltDown())
			Flags |= libMozilla::Window::ALT;
		if(msg.Modifiers.isControlDown())
			Flags |= libMozilla::Window::CONTROL;
		if(msg.Modifiers.isShiftDown())
			Flags |= libMozilla::Window::SHIFT;

		switch(msg.Type)
		{
		case UIMessage::MouseMove:
			{
				m_MozillaWindow->onMouseMove(msg.MouseCoords.x, msg.MouseCoords.y, Flags);
				return true;
			}
			break;
		case UIMessage::LeftMouseDown:
			{
				m_MozillaWindow->onLeftMouseDown(msg.MouseCoords.x, msg.MouseCoords.y, Flags);
				return true;
			}
			break;
		case UIMessage::LeftMouseUp:
			{
				m_MozillaWindow->onLeftMouseUp(msg.MouseCoords.x, msg.MouseCoords.y, Flags);
				return true;
			}
			break;
		case UIMessage::MiddleMouseDown:
			{
				m_MozillaWindow->onMiddleMouseDown(msg.MouseCoords.x, msg.MouseCoords.y, Flags);
				return true;
			}
			break;
		case UIMessage::MiddleMouseUp:
			{
				m_MozillaWindow->onMiddleMouseUp(msg.MouseCoords.x, msg.MouseCoords.y, Flags);
				return true;
			}
			break;
		case UIMessage::RightMouseDown:
			{
				m_MozillaWindow->onRightMouseDown(msg.MouseCoords.x, msg.MouseCoords.y, Flags);
				return true;
			}
			break;
		case UIMessage::RightMouseUp:
			{
				m_MozillaWindow->onRightMouseUp(msg.MouseCoords.x, msg.MouseCoords.y, Flags);
				return true;
			}
			break;
		case UIMessage::MouseWheel:
			{
				m_MozillaWindow->onMouseWheel(msg.Data, Flags);
				return true;
			}
			break;

		case UIMessage::KeyUp:
			return true;

		case UIMessage::KeyDown:
		case UIMessage::KeyRepeat:
			{
				m_MozillaWindow->onKeyPress( 0, 0xFFFF & msg.Keystroke, Flags );
				return true;
			}
		default:
			break;
		}
	}

	return true;
}

void SwgCuiWebBrowserWidget::SetSelected(const bool selected)
{		
	if(IsSelected() != selected)
	{
		UIWidget::SetSelected(selected);

		if(m_MozillaWindow)
			m_MozillaWindow->setFocus(selected);

		if(!selected)
			::SetFocus(Os::getWindow());
	}
}

void SwgCuiWebBrowserWidget::NavigateForward()
{
	if(m_MozillaWindow && m_MozillaWindow->canNavigateForward())
		m_MozillaWindow->navigateForward();
		
}

void SwgCuiWebBrowserWidget::NavigateBack()
{
	if(m_MozillaWindow && m_MozillaWindow->canNavigateBack())
		m_MozillaWindow->navigateBack();
}

void SwgCuiWebBrowserWidget::NavigateStop()
{
	if(m_MozillaWindow)
		m_MozillaWindow->navigateStop();
}

void SwgCuiWebBrowserWidget::createMozillaWindow()
{
   if(!m_MozillaWindow)
	   m_MozillaWindow = getMozillaWindow();
}

void SwgCuiWebBrowserWidget::setIntegrationProbeNonce(char const * nonce)
{
	m_integrationProbeNonce = nonce ? nonce : "";
	m_reportedIntegrationProxyWindow = false;
	m_reportedIntegrationFirstNonzeroFrame = false;
}

void SwgCuiWebBrowserWidget::RefreshPage()
{
	if(m_MozillaWindow)
		m_MozillaWindow->reload();
}

void SwgCuiWebBrowserWidget::setHomePage(std::string const & home)
{
	s_homePage = home;
}

std::string SwgCuiWebBrowserWidget::getCurrentURL() const
{
	if(m_MozillaWindow)
	{
		return std::string(m_MozillaWindow->getURI());
	}

	return std::string();
}
#endif
// ======================================================================
