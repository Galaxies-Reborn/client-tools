// ======================================================================
//
// SwgCuiTcgWindow.cpp
// copyright (c) 2008 Sony Online Entertainment LLC
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiTcgWindow.h"

#include "clientAudio/Audio.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiMessageBox.h"
#include "clientUserInterface/CuiStringIds.h"
#include "clientUserInterface/CuiWorkspaceIcon.h"
#include "sharedFoundation/Os.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "swgClientUserInterface/SwgCuiTcgControl.h"

#include "UIImage.h"

#include <algorithm>
#include <cmath>
#include <limits>

// ======================================================================

namespace
{
#if defined(_WIN64)
	bool mapNativeWindowRectToControl(
		SwgCuiTcgControl const & mainControl,
		libEverQuestTCG::Window & mainWindow,
		UIPage const & pageParent,
		int nativeX,
		int nativeY,
		unsigned nativeWidth,
		unsigned nativeHeight,
		UIPoint & mappedLocation,
		UISize & mappedSize)
	{
		int const controlWidth = mainControl.GetWidth();
		int const controlHeight = mainControl.GetHeight();
		if (controlWidth <= 0 || controlHeight <= 0 || nativeWidth == 0 || nativeHeight == 0)
			return false;

		void * surfaceBits = 0;
		unsigned surfaceWidth = 0;
		unsigned surfaceHeight = 0;
		unsigned surfaceStride = 0;
		if (!mainWindow.getWindowSurfaceData(&surfaceBits, &surfaceWidth, &surfaceHeight, &surfaceStride) ||
			!surfaceBits || surfaceWidth == 0 || surfaceHeight == 0 ||
			static_cast<unsigned long long>(surfaceWidth) * 4u > surfaceStride)
		{
			return false;
		}

		int mainNativeX = 0;
		int mainNativeY = 0;
		unsigned mainNativeWidth = 0;
		unsigned mainNativeHeight = 0;
		mainWindow.getRect(mainNativeX, mainNativeY, mainNativeWidth, mainNativeHeight);
		UNREF(mainNativeWidth);
		UNREF(mainNativeHeight);

		UIPoint const controlWorldLocation = mainControl.GetWorldLocation();
		UIPoint const parentWorldLocation = pageParent.GetWorldLocation();
		double const scaleX = static_cast<double>(controlWidth) / static_cast<double>(surfaceWidth);
		double const scaleY = static_cast<double>(controlHeight) / static_cast<double>(surfaceHeight);
		double const nativeLeftOffset = static_cast<double>(static_cast<long long>(nativeX) - mainNativeX);
		double const nativeTopOffset = static_cast<double>(static_cast<long long>(nativeY) - mainNativeY);
		double const nativeRightOffset = nativeLeftOffset + nativeWidth;
		double const nativeBottomOffset = nativeTopOffset + nativeHeight;

		double const mappedLeftWorld = std::floor(controlWorldLocation.x + nativeLeftOffset * scaleX);
		double const mappedTopWorld = std::floor(controlWorldLocation.y + nativeTopOffset * scaleY);
		double const mappedRightWorld = std::ceil(controlWorldLocation.x + nativeRightOffset * scaleX);
		double const mappedBottomWorld = std::ceil(controlWorldLocation.y + nativeBottomOffset * scaleY);
		double const mappedLocalX = mappedLeftWorld - parentWorldLocation.x;
		double const mappedLocalY = mappedTopWorld - parentWorldLocation.y;
		double const mappedWidth = mappedRightWorld - mappedLeftWorld;
		double const mappedHeight = mappedBottomWorld - mappedTopWorld;
		double const longMinimum = static_cast<double>(std::numeric_limits<long>::min());
		double const longMaximum = static_cast<double>(std::numeric_limits<long>::max());
		if (!std::isfinite(mappedLocalX) || !std::isfinite(mappedLocalY) ||
			!std::isfinite(mappedWidth) || !std::isfinite(mappedHeight) ||
			mappedLocalX < longMinimum || mappedLocalX > longMaximum ||
			mappedLocalY < longMinimum || mappedLocalY > longMaximum ||
			mappedWidth < 1.0 || mappedWidth > longMaximum ||
			mappedHeight < 1.0 || mappedHeight > longMaximum)
		{
			return false;
		}

		mappedLocation = UIPoint(static_cast<long>(mappedLocalX), static_cast<long>(mappedLocalY));
		mappedSize = UISize(static_cast<long>(mappedWidth), static_cast<long>(mappedHeight));
		return true;
	}
#endif
}

// ======================================================================

SwgCuiTcgWindow::SwgCuiTcgWindow(UIPage & page)
: CuiMediator("SwgCuiTcgWindow", page)
, m_tcgControl(0)
, m_tcgPage(0)
, m_tcgParent(0)
, m_callbacks ( new MessageDispatch::Callback )
{
	IGNORE_RETURN(setState(MS_closeable));
	IGNORE_RETURN(setState(MS_closeDeactivates));
}

// ----------------------------------------------------------------------

SwgCuiTcgWindow::~SwgCuiTcgWindow()
{
	m_tcgControl = 0;
	m_tcgPage = 0;
	m_tcgParent = 0;

	delete m_callbacks;
	m_callbacks = 0;
}

// ----------------------------------------------------------------------

bool SwgCuiTcgWindow::dispatchIntegrationTestClick(
	unsigned normalizedX,
	unsigned normalizedWidth,
	unsigned normalizedY,
	unsigned normalizedHeight)
{
	return m_tcgControl && m_tcgControl->dispatchIntegrationTestClick(
		normalizedX, normalizedWidth, normalizedY, normalizedHeight);
}

// ----------------------------------------------------------------------

void SwgCuiTcgWindow::performActivate()
{
	CuiManager::requestPointer(true);
	CuiManager::requestKeyboard(true);

	if(!m_tcgPage || !m_tcgControl)
		createTcgControl();

	if (m_tcgControl)
	{
		m_tcgControl->initializeEqTcgWindow();

		if (m_windows.empty())
		{
			libEverQuestTCG::Window* mainWindow = m_tcgControl->getEqTcgWindow();

			if (mainWindow)
			{
				Window newWindow;
				newWindow.pPage = NULL;
				newWindow.pTCGControl = m_tcgControl;

				mainWindow->setUserData(m_tcgControl);

				m_windows.push_back(newWindow);
			}

		}
	}

	setIsUpdating(true);

	Audio::silenceNonBufferedMusic(true);
}

// ----------------------------------------------------------------------

void SwgCuiTcgWindow::performDeactivate()
{
	CuiManager::requestPointer(false);
	CuiManager::requestKeyboard(false);

	setIsUpdating(false);
}

// ----------------------------------------------------------------------

bool SwgCuiTcgWindow::close()
{

	if (m_tcgControl && !m_tcgControl->getEqTcgWindow()) // User clicked EXIT via TCG game. They've already gotten a "are you sure?" message.
	{
		// Window is already closed, no need to do anything.
	}
	else
	{
		CuiMessageBox * const box = CuiMessageBox::createYesNoBox (CuiStringIds::tcg_exit_confirmation.localize());
		m_callbacks->connect (box->getTransceiverClosed (), *this, &SwgCuiTcgWindow::onMessageBoxClose);
		return true; // This will prevent the Workspace from closing us. We'll handle it later.
	}

	Audio::silenceNonBufferedMusic(false);

	return true;
}

// ----------------------------------------------------------------------

void SwgCuiTcgWindow::createTcgControl()
{
	UIPage * const tcgControlParent = NON_NULL(safe_cast<UIPage *>(getPage().GetChild("tcgControlParent")));

	if (tcgControlParent)
	{
		m_tcgPage = tcgControlParent;
		UIBaseObject * const tcgPageParent = m_tcgPage->GetParent();
		m_tcgParent = tcgPageParent ? tcgPageParent->GetParent() : 0;

		FATAL(!m_tcgParent, ("m_tcgParent is null!"));

		if(m_tcgControl) // Our TCG Control was already created.
			return;

		m_tcgControl = new SwgCuiTcgControl;

		NOT_NULL(m_tcgControl);

		if (m_tcgControl)
		{
			m_tcgControl->SetGetsInput(true);
			m_tcgControl->SetSelectable(true);
			m_tcgControl->SetVisible(true);

			m_tcgControl->SetSize(tcgControlParent->GetSize());

#ifdef _DEBUG
			char buffer[256];
			memset(buffer, 0, 256);
			sprintf(buffer, "TCG Main Control");
			m_tcgControl->SetName(buffer);
#endif

			tcgControlParent->AddChild(m_tcgControl);
			m_tcgControl->SetProperty(UIWidget::PropertyName::PackSize, Unicode::narrowToWide("a,a"));

			UIImage * tcgImage = 0;
			getCodeDataObject(TUIImage, tcgImage, "tcgImage");

			m_tcgControl->setImage(tcgImage);

			m_tcgControl->initializeEqTcgWindow();

			libEverQuestTCG::Window* mainWindow = m_tcgControl->getEqTcgWindow();

			if (mainWindow)
			{
				Window newWindow;
				newWindow.pPage = NULL;
				newWindow.pTCGControl = m_tcgControl;

				mainWindow->setUserData(m_tcgControl);

				m_windows.push_back(newWindow);
			}
		}
	}
}

// ----------------------------------------------------------------------

void SwgCuiTcgWindow::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);

	if (!m_tcgControl)
		return;

	if (!m_tcgControl->getEqTcgWindow())
		closeNextFrame();
	else
		m_tcgControl->alter(deltaTimeSecs);

	if (!m_tcgPage)
		return;

	unsigned uWindows = libEverQuestTCG::getWindows(0,0);
	unsigned uWindowsRemoved = 0;

	libEverQuestTCG::Window **ppWindows = 0;

	if( uWindows > 0 )
	{
		ppWindows = (libEverQuestTCG::Window **)alloca( uWindows * sizeof( libEverQuestTCG::Window * ) );
		libEverQuestTCG::getWindows( ppWindows, uWindows );
	}

	libEverQuestTCG::Window **ppWindowsEnd = ppWindows + uWindows;

	libEverQuestTCG::Window * m_pMainTCGWindow = m_tcgControl->getEqTcgWindow();

	if( m_pMainTCGWindow ) // Still need to go in here if 0 == uWindows so m_pMainTCGWindow gets cleared
	{
		// Verify main window still exists
		if( ppWindowsEnd == std::find( ppWindows, ppWindowsEnd, m_pMainTCGWindow ) )
		{
			while( ppWindowsEnd != ppWindows )
			{
				--ppWindowsEnd;
				(*ppWindowsEnd)->close();
			}

			uWindows = 0;
			m_pMainTCGWindow = 0;
		}
	}

	bool forceFocus = false;

	// Look for deleted windows
	for( unsigned i = 0; i != m_windows.size(); /* increment in body */ )
	{
		Window &rWindow = m_windows[i];
		libEverQuestTCG::Window *pWindow = rWindow.pTCGControl->getEqTcgWindow();
		libEverQuestTCG::Window **ppWindow = std::find( ppWindows, ppWindowsEnd, pWindow );

		// Get the window page
		if( ppWindow == ppWindowsEnd )
		{
			// @TODO: The sliding menu pop-up windows can get into a state where they need to be closed by a left mouse button up message
			if (pWindow && pWindow != m_pMainTCGWindow && pWindow->canGetFocus())
				pWindow->onLeftMouseUp(-1, -1, -1, -1, 0);

			rWindow.pTCGControl->setEqTcgWindow( 0 );

			if(rWindow.pPage)
				m_tcgParent->RemoveChild( rWindow.pPage );

			m_windows.erase( m_windows.begin() + i );
			++uWindowsRemoved;

			forceFocus = true;
		}
		else
		{
			if( m_pMainTCGWindow != pWindow )
			{
				// Sync up the rects with anything that might have happened
				int x, y;
				unsigned w, h;
				pWindow->getRect( x, y, w, h );
				UIPoint targetLocation(x, y);
				UISize targetSize(w, h);
#if defined(_WIN64)
				UIPage const * const parentPage = safe_cast<UIPage const *>(m_tcgParent);
				if (parentPage)
				{
					IGNORE_RETURN(mapNativeWindowRectToControl(
						*m_tcgControl, *m_pMainTCGWindow, *parentPage,
						x, y, w, h, targetLocation, targetSize));
				}
#endif

#ifdef _DEBUG
				UISize pageSize = rWindow.pPage->GetSize();
				UIPoint pageLoc = rWindow.pPage->GetLocation();

				DEBUG_REPORT_PRINT(true, ("Wx = %d, Wy = %d, Ww = %d, Wh = %d\n", x, y, w, h));
				DEBUG_REPORT_PRINT(true, ("Px = %d, Py = %d, Pw = %d, Ph = %d\n", pageLoc.x, pageLoc.y, pageSize.x, pageSize.y));
				DEBUG_REPORT_PRINT(true, ("Tx = %d, Ty = %d, Tw = %d, Th = %d\n",
					targetLocation.x, targetLocation.y, targetSize.x, targetSize.y));
#endif
				if (rWindow.pPage->GetSize() != targetSize)
				{
					DEBUG_REPORT_PRINT(true, ("Sizing page 0x%08X to %d, %d", rWindow.pPage, targetSize.x, targetSize.y));
					rWindow.pPage->SetSize(targetSize);
					rWindow.pTCGControl->SetSize(targetSize);
				}

				if (rWindow.pPage->GetLocation() != targetLocation)
				{
					DEBUG_REPORT_PRINT(true, ("Moving page 0x%08X to %d, %d", rWindow.pPage, targetLocation.x, targetLocation.y));
					rWindow.pPage->SetLocation(targetLocation);
				}
			}

			++i;
		}
	}

	// Look for new windows
	for( unsigned i = 0; i != uWindows; ++i )
	{
		libEverQuestTCG::Window *pWindow = ppWindows[ i ];

		if( SwgCuiTcgControl *pTCGControl = reinterpret_cast< SwgCuiTcgControl * >( pWindow->getUserData() ) )
		{
			// Existing window, push location into lib
			UIPoint pt( pTCGControl->GetWorldLocation() );
#if defined(_WIN64)
			// Child pages are scaled into the main control's coordinate space. Their
			// native host positions remain authoritative and must not be overwritten
			// with the scaled UI positions.
			if (pWindow == m_pMainTCGWindow)
				pWindow->setLocation(pt.x, pt.y);
#else
			pWindow->setLocation( pt.x, pt.y );
#endif
		}
		else
		{
			// don't let QT get mouse capture
			if (::GetCapture() != Os::getWindow())
				ReleaseCapture();

			// This is a new window

			int iX, iY;
			unsigned uWidth, uHeight;
			pWindow->getRect( iX, iY, uWidth, uHeight );
			UIPoint targetLocation(iX, iY);
			UISize targetSize(uWidth, uHeight);
#if defined(_WIN64)
			UIPage const * const parentPage = safe_cast<UIPage const *>(m_tcgParent);
			if (m_pMainTCGWindow && pWindow != m_pMainTCGWindow && parentPage)
			{
				IGNORE_RETURN(mapNativeWindowRectToControl(
					*m_tcgControl, *m_pMainTCGWindow, *parentPage,
					iX, iY, uWidth, uHeight, targetLocation, targetSize));
			}
#endif

			Window newWindow;
			newWindow.pTCGControl = new SwgCuiTcgControl;

			DEBUG_REPORT_LOG(false, ("Create - CanGetFocus = %s\n", pWindow->canGetFocus() ? "true" : "false"));

			if (pWindow->canGetFocus())
			{
				newWindow.pTCGControl->SetGetsInput(true);
				newWindow.pTCGControl->SetSelectable(true);

				newWindow.pTCGControl->SetSelected(true);
				newWindow.pTCGControl->SetFocus();

				static_cast<UIPage *>(m_tcgParent)->GiveWidgetMouseLock(newWindow.pTCGControl);
			}
			else
			{
				newWindow.pTCGControl->SetGetsInput(false);
				newWindow.pTCGControl->SetSelectable(false);
			}

			newWindow.pTCGControl->setEqTcgWindow( pWindow );
			newWindow.pTCGControl->SetSize(targetSize);
			newWindow.pTCGControl->SetVisible( true );

#ifdef _DEBUG
			char buffer[256];
			memset(buffer, 0, 256);
			sprintf(buffer, "TCG Child Control #%d", i);
			newWindow.pTCGControl->SetName(buffer);
#endif
			
			UIImage * tcgImage = 0;
			getCodeDataObject(TUIImage, tcgImage, "tcgImageContext");

			newWindow.pTCGControl->setImage(tcgImage);
			
			pWindow->setUserData( newWindow.pTCGControl );

			bool bNaked = true;

			if( bNaked )
			{
				newWindow.pPage = new UIPage;
				newWindow.pPage->SetVisible( true );
				newWindow.pPage->SetSize(targetSize);
				
				newWindow.pPage->SetLocation(targetLocation);

				newWindow.pPage->AddChild( newWindow.pTCGControl );
			}

			m_tcgParent->AddChild(newWindow.pPage);
			m_tcgParent->MoveChild(newWindow.pPage, UIBaseObject::Top);

			/*
			static bool dofocus = false;
			if(dofocus)
			if( m_tcgPage->IsSelected() && pWindow->canGetFocus() )
				newWindow.pTCGControl->SetFocus();

			m_tcgControl->SetFocus();
			*/

			m_windows.push_back( newWindow );

			//if( pWindow == m_pMainTCGWindow )
			//{
			//	UISize rootSize = static_cast< UIWidget * >( m_tcgPage->GetRoot() )->GetSize();
			//	UISize controlSize = newWindow.pTCGControl->GetSize();
			//	if( controlSize.x >= rootSize.x && controlSize.y >= rootSize.y )
			//		m_maximize(true);

			//	// Main window always gets focus
			//	newWindow.pTCGControl->SetFocus();
			//}
		}
	}

	// Always give the first child window mouse lock when there are any child windows open as long as its not a tooltip
	if (uWindows > 1 && ppWindows)
	{
		libEverQuestTCG::Window * pWindow = ppWindows[1];

		if (pWindow && pWindow->canGetFocus() && pWindow->getUserData())
		{
			UIWidget * const widget = static_cast<UIWidget *>(pWindow->getUserData());

			if (widget)
			{
				widget->SetSelected(true);
				widget->SetFocus();
				static_cast<UIPage *>(m_tcgParent)->GiveWidgetMouseLock(static_cast<UIWidget *>(widget));
			}
		}
	}

	if (forceFocus && m_tcgControl && m_tcgControl->getEqTcgWindow())
	{
		static_cast<UIPage *>(m_tcgParent)->ReleaseMouseLock(UIPoint(0,0));

		m_tcgControl->SetSelected(true);
		m_tcgControl->SetFocus();
		m_tcgControl->getEqTcgWindow()->setFocus(true);
	}
}

void SwgCuiTcgWindow::onMessageBoxClose(const CuiMessageBox& box)
{
	if(box.completedAffirmative())
	{
		if(m_tcgControl && m_tcgControl->getEqTcgWindow())
		    m_tcgControl->getEqTcgWindow()->close();

	   //m_tcgPage = 0;

	   //Audio::silenceNonBufferedMusic(false);

	   //CuiMediator::close();

	   //activate();
	}
}

// ======================================================================
