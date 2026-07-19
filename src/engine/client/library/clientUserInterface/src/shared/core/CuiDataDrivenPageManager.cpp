// ======================================================================
//
// CuiDataDrivenPageManager.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "clientUserInterface/FirstClientUserInterface.h"
#include "clientUserInterface/CuiDataDrivenPageManager.h"

#include "sharedMessageDispatch/Emitter.h"
#include "sharedMessageDispatch/Message.h"
#include "sharedMessageDispatch/Receiver.h"
#include "sharedNetworkMessages/ServerUserInterfaceMessages.h"
#include "sharedNetworkMessages/SuiCreatePageMessage.h"
#include "sharedNetworkMessages/SuiUpdatePageMessage.h"

#include "clientGame/Game.h"
#include "clientGame/GameNetwork.h"
#include "clientUserInterface/CuiDataDrivenPage.h"
#include "clientUserInterface/CuiDataDrivenPageCountdownTimer.h"
#include "clientUserInterface/CuiMediator.h"
#include "clientUserInterface/CuiWorkspace.h"

#include "UIBaseObject.h"
#include "UIData.h"
#include "UIDataSource.h"
#include "UIButton.h"
#include "UIList.h"
#include "UIManager.h"
#include "UiPage.h"
#include "UIText.h"
#include "Unicode.h"
#include "UnicodeUtils.h"

#include <map>
#include <vector>

//-----------------------------------------------------------------

namespace CuiDataDrivenPageManagerNamespace
{
	bool s_installed = false;

	//NOTE: We need a seperate class to handle the Receiver, since it's non-static and 
	//      our manager has to be static to fit with the other CuiManagers (i.e. be installed from within a static function)
	class Listener : public MessageDispatch::Receiver
	{
	public:

		Listener()
		:  MessageDispatch::Receiver()
		{
			connectToMessage (SuiCreatePageMessage::MessageType);
			connectToMessage ("SuiForceClosePage");
			connectToMessage (SuiUpdatePageMessage::MessageType);
			connectToMessage(Game::Messages::SCENE_CHANGED);
		}

		//----------------------------------------------------------------------

		void receiveMessage(const MessageDispatch::Emitter & source, const MessageDispatch::MessageBase & message)
		{
			UNREF(source);
			if (message.isType (SuiCreatePageMessage::MessageType))
			{
				Archive::ReadIterator ri = dynamic_cast<const GameNetworkMessage *>(&message)->getByteStream().begin();
				SuiCreatePageMessage scp(ri);
				CuiDataDrivenPageManager::receiveCreatePageMessage(scp);
			}
			else if (message.isType ("SuiForceClosePage"))
			{
				Archive::ReadIterator ri = dynamic_cast<const GameNetworkMessage &>(message).getByteStream().begin();
				SuiForceClosePage sfcp(ri);
				CuiDataDrivenPageManager::receiveForceCloseMessage(sfcp);
			}
			else if (message.isType (SuiUpdatePageMessage::MessageType))
			{
				Archive::ReadIterator ri = dynamic_cast<const GameNetworkMessage &>(message).getByteStream().begin();
				SuiUpdatePageMessage sup(ri);				
				CuiDataDrivenPageManager::receiveUpdatePageMessage(sup);
			}
			else if(message.isType(Game::Messages::SCENE_CHANGED))
			{
				CuiDataDrivenPageManager::handleSceneChange();
			}
		}
	};

	Listener * s_listener = 0;

	const std::string sc_titleLocation = "bg.caption.lblTitle";
}

using namespace CuiDataDrivenPageManagerNamespace;

//-----------------------------------------------------------------

bool CuiDataDrivenPageManager::ms_installed;
std::map<int, CuiDataDrivenPage*>CuiDataDrivenPageManager::ms_pages;

//-----------------------------------------------------------------

void CuiDataDrivenPageManager::install()
{
	DEBUG_FATAL (ms_installed, ("already installed\n"));
	s_listener = new Listener;
	ms_installed = true;
}

//-----------------------------------------------------------------

void CuiDataDrivenPageManager::remove()
{
	DEBUG_FATAL (!ms_installed, ("not installed\n"));
	delete s_listener;
	s_listener   = 0;
	ms_installed = false;
	for (PageMap::iterator it = ms_pages.begin(); it != ms_pages.end(); ++it)
		(*it).second->release ();
}

//-----------------------------------------------------------------

/**
 *  Given a UI page and a number of commands to run, build a new UI page and display it
 */
void CuiDataDrivenPageManager::createPage (SuiPageData const &pageData)										   
{
	std::string const & pageName = pageData.getPageName();
	int const clientPageId = pageData.getPageId();

	//get the template
	UIPage const * const prototype = NON_NULL (safe_cast<UIPage const *>(UIManager::gUIManager().GetObjectFromPath(pageName.c_str(), TUIPage)));

	if(!prototype)
	{
		WARNING (true, ("Bad DataDriven page name <%s> given", pageName.c_str()));
		return;
	}

	//copy the "template" into the page that we'll use
	UIPage* const newPage = dynamic_cast<UIPage*>(NON_NULL(prototype->DuplicateObject()));
	DEBUG_FATAL(!newPage, ("Couldn't find base page in CuiDataDrivenPageManager::createPage"));
	NOT_NULL(newPage);

	//create the mediator for the page
	CuiDataDrivenPage * mediator = NULL;

	if (_stricmp(pageName.c_str(), "Script.CountdownTimerBar") == 0)
		mediator = new CuiDataDrivenPageCountdownTimer(std::string(), *newPage, clientPageId);
	else
		mediator = new CuiDataDrivenPage(std::string(), *newPage, clientPageId);
	
	mediator->processPageData(pageData);

	ms_pages[clientPageId] = mediator;
	mediator->fetch ();

	//add the page to the game  workspace (like a HUD)

	CuiWorkspace * const workspace = CuiWorkspace::getGameWorkspace();
	if (workspace)
	{
		workspace->addMediator(*mediator);
		mediator->activate();
		mediator->setEnabled (true);
		workspace->focusMediator (*mediator, true);
	}

	newPage->Link();
}

//-----------------------------------------------------------------

void CuiDataDrivenPageManager::closePage(int pageId)
{
	PageMap::iterator const i = ms_pages.find(pageId);
	if(i != ms_pages.end())
		removePage(i->second, false);
	else
		DEBUG_WARNING(true, ("Couldn't call closePage on pageId:%d", pageId));
}

//-----------------------------------------------------------------

void CuiDataDrivenPageManager::removePage(CuiDataDrivenPage* page, bool alreadyClosing)
{
	if (ms_pages.find(page->getClientPageId()) == ms_pages.end())
	{
		DEBUG_WARNING(true, ("Bad page* sent to CuiDataDrivenPageManager::removePage"));
		return;
	}

	IGNORE_RETURN(ms_pages.erase(page->getClientPageId()));

	if(!alreadyClosing)
	{
		page->closeThroughWorkspace();
	}
	page->release();
}

//-----------------------------------------------------------------

int CuiDataDrivenPageManager::selectOrConfirmSingleListRow(
	int const row, bool const confirm)
{
	if (row < 0)
		return 0x10000;

	CuiDataDrivenPage * candidate = 0;
	UIList * candidateList = 0;
	UIButton * candidateOk = 0;
	bool candidatePromptUpdated = false;
	int observations = 0;
	for (PageMap::iterator iterator = ms_pages.begin(); iterator != ms_pages.end(); ++iterator)
	{
		CuiDataDrivenPage * const page = iterator->second;
		if (!page || !page->isActive() || page->isClosed())
			continue;
		observations |= 0x001;

		UIList * const list = dynamic_cast<UIList *>(
			page->getPage().GetObjectFromPath("List.lstList", TUIList));
		if (list)
			observations |= 0x002;
		UIButton * const ok = dynamic_cast<UIButton *>(
			page->getPage().GetObjectFromPath("btnOk", TUIButton));
		if (ok)
			observations |= 0x004;
		if (ok && ok->IsEnabled())
			observations |= 0x008;
		UIText const * const prompt = dynamic_cast<UIText const *>(
			page->getPage().GetObjectFromPath("Prompt.lblPrompt", TUIText));
		if (prompt)
			observations |= 0x010;
		UIString promptText;
		UIString promptLocalText;
		std::string const clonePromptPrefix("@base_player:clone_prompt_header");
		bool const hasPromptText =
			prompt &&
			prompt->GetProperty(UIText::PropertyName::Text, promptText);
		bool const hasPromptLocalText =
			prompt &&
			prompt->GetProperty(UIText::PropertyName::LocalText, promptLocalText);
		if (hasPromptText)
			observations |= 0x020;
		if (hasPromptLocalText)
			observations |= 0x040;
		bool const isClonePrompt =
			(hasPromptText &&
				Unicode::wideToNarrow(promptText).compare(
					0, clonePromptPrefix.size(), clonePromptPrefix) == 0) ||
			(hasPromptLocalText &&
				Unicode::wideToNarrow(promptLocalText).find(
					"base_player:clone_prompt_header") != std::string::npos);
		if (isClonePrompt)
			observations |= 0x080;
		bool const hasClonePromptData =
			(hasPromptText &&
				Unicode::wideToNarrow(promptText).find(
					"base_player:clone_prompt_data") != std::string::npos) ||
			(hasPromptLocalText &&
				Unicode::wideToNarrow(promptLocalText).find(
					"base_player:clone_prompt_data") != std::string::npos);
		if (hasClonePromptData)
			observations |= 0x400;
		if (!list || !ok || !ok->IsEnabled() || !prompt ||
			!isClonePrompt)
			continue;

		UIDataSource const * const dataSource = list->GetDataSource();
		if (dataSource)
			observations |= 0x100;
		if (dataSource && row < static_cast<int>(dataSource->GetChildCount()))
			observations |= 0x200;
		if (!dataSource || row >= static_cast<int>(dataSource->GetChildCount()))
			continue;

		// Refuse an ambiguous generic-listbox state instead of choosing a
		// potentially unrelated SUI page.
		if (candidate)
			return 0x20000 | observations;
		candidate = page;
		candidateList = list;
		candidateOk = ok;
		candidatePromptUpdated = hasClonePromptData;
	}

	if (!candidate || !candidateList || !candidateOk)
		return 0x10000 | observations;

	if (!confirm)
	{
		// SelectRow emits the normal OnListSelectionChanged callback, which
		// sends the subscribed SelectedRow property to the server. A generic
		// list can arrive with row zero already selected; clear that default
		// locally without a callback so selecting row zero still emits the
		// one authoritative selection event a physical click would produce.
		if (candidateList->IsRowSelected(row))
			candidateList->SelectRow(-1, false);
		candidateList->SelectRow(row);
		if (!candidateList->IsRowSelected(row) ||
			!candidateList->GetDataAtRow(row))
			return 0x30000 | observations;
		return 1;
	}

	// Confirm only the same still-selected row after the server-authored
	// prompt update has had time to round-trip. Press() follows the same
	// listener path as a physical OK click.
	if (!candidatePromptUpdated)
		return 0x50000 | observations;
	if (!candidateList->IsRowSelected(row) ||
		!candidateList->GetDataAtRow(row))
		return 0x40000 | observations;
	candidateOk->Press();
	return 1;
}

//-----------------------------------------------------------------

void CuiDataDrivenPageManager::handleSceneChange()
{
	PageMap::iterator i;
	while(!ms_pages.empty())
	{
		i = ms_pages.begin();
		closePage(i->first);
	}
}

//-----------------------------------------------------------------

void CuiDataDrivenPageManager::receiveCreatePageMessage      (SuiCreatePageMessage const &createPageMessage)
{
	createPage(createPageMessage.getPageData());
}

//-----------------------------------------------------------------

void CuiDataDrivenPageManager::receiveForceCloseMessage      (SuiForceClosePage const &forceClosePageMessage)
{	
	int const clientPageId = forceClosePageMessage.getClientPageId();
	CuiDataDrivenPageManager::closePage(clientPageId);
}

//-----------------------------------------------------------------

void CuiDataDrivenPageManager::receiveUpdatePageMessage      (SuiUpdatePageMessage const &updatePageMessage)
{
	PageMap::iterator const i = ms_pages.find(updatePageMessage.getPageData().getPageId());
	if(i == ms_pages.end())
	{
		DEBUG_WARNING(true, ("Couldn't find page to update"));
		return;
	}
	CuiDataDrivenPage* const page = i->second;
	page->processPageData(updatePageMessage.getPageData());
}

//-----------------------------------------------------------------
