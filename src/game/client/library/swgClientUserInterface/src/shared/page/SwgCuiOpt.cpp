//======================================================================
//
// SwgCuiOpt.cpp
// copyright (c) 2003 Sony Online Entertainment
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiOpt.h"

#include "UIBaseObject.h"
#include "UIButton.h"
#include "UIData.h"
#include "UIDataSource.h"
#include "UIPage.h"
#include "UITabbedPane.h"
#include "UIText.h"
#include "UnicodeUtils.h"
#include "clientGame/Game.h"
#include "clientUserInterface/CuiSettings.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiTextManager.h"
#include "sharedUtility/CurrentUserOptionManager.h"
#include "sharedUtility/LocalMachineOptionManager.h"
#include "swgClientUserInterface/SwgCuiOptAfk.h"
#include "swgClientUserInterface/SwgCuiOptAlarm.h"
#include "swgClientUserInterface/SwgCuiOptChat.h"
#include "swgClientUserInterface/SwgCuiOptChatColors.h"
#include "swgClientUserInterface/SwgCuiOptCombat.h"
#include "swgClientUserInterface/SwgCuiOptControls.h"
#include "swgClientUserInterface/SwgCuiOptGraphics.h"
#include "swgClientUserInterface/SwgCuiOptKeymap.h"
#include "swgClientUserInterface/SwgCuiOptMisc.h"
#include "swgClientUserInterface/SwgCuiOptPerformance.h"
#include "swgClientUserInterface/SwgCuiOptSound.h"
#include "swgClientUserInterface/SwgCuiOptTerrain.h"
#include "swgClientUserInterface/SwgCuiOptUi.h"
#include "swgClientUserInterface/SwgCuiOptVoice.h"
#include <map>
#include <vector>

//======================================================================

namespace SwgCuiOptNamespace
{
	enum OptionTabs
	{
		OT_afk,
		OT_alarm,
		OT_chat,
		OT_chatColors,
		OT_combat,
		OT_controls,
		OT_graphics,
		OT_misc,
		OT_performance,
		OT_sound,
		OT_terrain,
		OT_ui,
		OT_keymap,
		//OT_voice,
		OT_numTabs
	};

	SwgCuiOptBase *s_swgCuiOpt = NULL;
}

using namespace SwgCuiOptNamespace;

//----------------------------------------------------------------------

SwgCuiOpt::SwgCuiOpt (UIPage & page) :
CuiMediator ("SwgCuiOpt", page),
UIEventCallback  (),
m_optionPages    (new BaseMap),
m_tabs           (0),
m_buttonOk       (0),
m_buttonCancel   (0),
m_keepSettings   (false)
{
	Game::setGameOptionChangedCallback(&gameOptionChanged);

	getCodeDataObject (TUITabbedPane, m_tabs,      "tabs");

	// Each lookup must zero optionPage first: getCodeDataObject leaves the
	// previous value in place on miss, so constructing a mediator after a
	// failed lookup would bind it to the WRONG page (causing every widget
	// inside the mediator to resolve against the wrong path and the tab to
	// render blank).
	UIPage * optionPage = 0;

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pageAfk", true);
	if (optionPage) (*m_optionPages) [OT_afk] = new SwgCuiOptAfk        (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pageAlarm", true);
	if (optionPage) (*m_optionPages) [OT_alarm] = new SwgCuiOptAlarm      (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pageChat", true);
	if (optionPage) (*m_optionPages) [OT_chat] = new SwgCuiOptChat       (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pageChatColors", true);
	if (optionPage) (*m_optionPages) [OT_chatColors] = new SwgCuiOptChatColors (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pageCombat", true);
	if (optionPage) (*m_optionPages) [OT_combat] = new SwgCuiOptCombat     (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pageControls", true);
	if (optionPage) (*m_optionPages) [OT_controls] = new SwgCuiOptControls   (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pageGraphics", true);
	if (optionPage) (*m_optionPages) [OT_graphics] = new SwgCuiOptGraphics   (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pageMisc", true);
	if (optionPage) (*m_optionPages) [OT_misc] = new SwgCuiOptMisc       (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pagePerformance", true);
	if (optionPage) (*m_optionPages) [OT_performance] = new SwgCuiOptPerformance (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pageSound", true);
	if (optionPage) (*m_optionPages) [OT_sound] = new SwgCuiOptSound      (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pageTerrain", true);
	if (optionPage) (*m_optionPages) [OT_terrain] = new SwgCuiOptTerrain    (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage,      "pageUi", true);
	if (optionPage) (*m_optionPages) [OT_ui] = new SwgCuiOptUi         (*optionPage);

	// do not show the "voice" page of settings as this feature is deprecated
	//getCodeDataObject (TUIPage, optionPage, "pageVoice");
	//(*m_optionPages) [OT_voice] = new SwgCuiOptVoice (*optionPage);

	optionPage = 0;
	getCodeDataObject (TUIPage, optionPage, "pageKeymap", true);
	if (optionPage) (*m_optionPages) [OT_keymap] = new SwgCuiOptKeymap (*optionPage, Game::getHudSceneType());

	// Strip tabs whose target widget doesn't exist in this UI bundle.
	// In NGE-retail OptMain the tab Target paths look like "target.misc",
	// which UITabbedPane resolves against its TargetPage (OptMain.comp).
	if (m_tabs)
	{
		UIDataSource * const tabDataSource = m_tabs->GetDataSource();
		UIBaseObject * const tabTargetBase = getPage().GetObjectFromPath("comp");
		if (tabDataSource && tabTargetBase)
		{
			long const tabCount = m_tabs->GetTabCount();
			for (long t = tabCount - 1; t >= 0; --t)
			{
				UIData * const tabData = m_tabs->GetTabData(t);
				if (!tabData)
					continue;

				UIString targetPath;
				if (!tabData->GetProperty(UITabbedPane::DataProperties::DATA_TARGET, targetPath))
					continue;

				std::string const narrowPath = Unicode::wideToNarrow(targetPath);
				UIBaseObject * const target = tabTargetBase->GetObjectFromPath(narrowPath.c_str(), TUIWidget);
				if (!target)
				{
					tabDataSource->RemoveChild(tabData);
				}
			}
		}
	}

	UIButton * buttonDefaultPage = 0;
	getCodeDataObject (TUIButton, buttonDefaultPage,      "buttonDefaultPage", true);
	if (buttonDefaultPage)
		buttonDefaultPage->Press ();

	for (BaseMap::iterator it = m_optionPages->begin (); it != m_optionPages->end (); ++it)
	{
		SwgCuiOptBase * const base = (*it).second;
		if(base)
		{		
			base->fetch ();
			base->getPage ().SetEnabled (true);
			base->getPage ().SetVisible (false);
		}
	}

	getCodeDataObject (TUIButton, m_buttonOk,     "buttonOk");
	getCodeDataObject (TUIButton, m_buttonCancel, "buttonCancel");

	registerMediatorObject (*m_buttonOk,      true);
	registerMediatorObject (*m_buttonCancel,  true);
	registerMediatorObject (*m_tabs,          true);

	setState (MS_closeable);
	setState (MS_closeDeactivates);
}

//----------------------------------------------------------------------

SwgCuiOpt::~SwgCuiOpt ()
{
	s_swgCuiOpt = NULL;

	for (BaseMap::iterator it = m_optionPages->begin (); it != m_optionPages->end (); ++it)
	{
		SwgCuiOptBase * const base = (*it).second;
		if(base)
			base->release ();
	}

	delete m_optionPages;
	m_optionPages = 0;

}

//----------------------------------------------------------------------

void SwgCuiOpt::gameOptionChanged()
{
	if (s_swgCuiOpt != NULL)
	{
		s_swgCuiOpt->queryWidgetValues();
	}
}

//----------------------------------------------------------------------

void SwgCuiOpt::performActivate   ()
{
	for (BaseMap::iterator it = m_optionPages->begin (); it != m_optionPages->end (); ++it)
	{
		SwgCuiOptBase * const base = (*it).second;
		if(base)
			base->storeRevertData ();
	}

	m_keepSettings = true;
	CuiManager::requestPointer (true);
	OnTabbedPaneChanged (m_tabs);
}

//----------------------------------------------------------------------

void SwgCuiOpt::performDeactivate ()
{
	CuiManager::requestPointer (false);

	for (BaseMap::iterator it = m_optionPages->begin (); it != m_optionPages->end (); ++it)
	{
		SwgCuiOptBase * const base = (*it).second;
		if(base)
			base->deactivate ();
	}
}

//----------------------------------------------------------------------

void SwgCuiOpt::OnButtonPressed   (UIWidget * context)
{
	if (context == m_buttonOk)
	{
		closeThroughWorkspace ();
	}
	else if (context == m_buttonCancel)
	{
		m_keepSettings = false;
		closeThroughWorkspace ();
	}
}

//----------------------------------------------------------------------

bool SwgCuiOpt::close()
{
	deactivate();

	if (m_keepSettings)
	{
		CuiSettings::save();
		CurrentUserOptionManager::save ();
		LocalMachineOptionManager::save ();
	}
	else
	{
		for (BaseMap::iterator it = m_optionPages->begin (); it != m_optionPages->end (); ++it)
		{
			SwgCuiOptBase * const base = (*it).second;
			if(base)
				base->revert ();
		}
	}

	for (BaseMap::iterator it = m_optionPages->begin (); it != m_optionPages->end (); ++it)
	{
		SwgCuiOptBase * const base = (*it).second;
		if(base)
			base->close ();
	}

	return CuiMediator::close ();
}

//----------------------------------------------------------------------

void SwgCuiOpt::OnTabbedPaneChanged (UIWidget * context)
{
	if (context == m_tabs)
	{
		const UIWidget * const activeWidget = m_tabs->GetActiveWidget ();

		for (BaseMap::iterator it = m_optionPages->begin (); it != m_optionPages->end (); ++it)
		{
			SwgCuiOptBase * const base = (*it).second;

			if(base)
			{
				if (&base->getPage () == activeWidget)
				{
					base->activate ();
					s_swgCuiOpt = (*it).second;
				}
				else
				{
					base->deactivate ();
				}
			}
		}
	}
}

//======================================================================
