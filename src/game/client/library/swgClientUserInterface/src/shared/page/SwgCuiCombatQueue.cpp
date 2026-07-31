//======================================================================
//
// SwgCuiCombatQueue.cpp
// copyright (c) 2002 Sony Online Entertainment
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiCombatQueue.h"

#include "clientGame/Game.h"
#include "clientUserInterface/CuiAction.h"
#include "clientUserInterface/CuiActionManager.h"
#include "clientUserInterface/CuiCombatManager.h"
#include "clientUserInterface/CuiDragInfo.h"
#include "clientUserInterface/CuiDragInfoTypes.h"
#include "clientUserInterface/CuiIconManager.h"
#include "clientUserInterface/CuiSkillManager.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "swgClientUserInterface/SwgCuiActions.h"
#include "UIButton.h"
#include "UIImage.h"
#include "UIImageStyle.h"
#include "UIPage.h"
#include "UIText.h"
#include "UIVolumePage.h"

//======================================================================

class SwgCuiCombatQueue::Action : public CuiAction
{
public:

	explicit Action(SwgCuiCombatQueue & mediator) :
	CuiAction(),
	m_mediator(mediator)
	{
	}

	virtual ~Action()
	{
		CuiActionManager::removeAction(this);
	}

	virtual bool performAction(std::string const & id, Unicode::String const &) const
	{
		if (id == SwgCuiActions::combatQueueCollapse)
		{
			m_mediator.setExpanded(false);
			return true;
		}
		if (id == SwgCuiActions::combatQueueExpand)
		{
			m_mediator.setExpanded(true);
			return true;
		}
		if (id == SwgCuiActions::clearCombatQueue)
		{
			m_mediator.clearCombatQueue();
			return true;
		}

		return false;
	}

private:

	Action();
	Action(Action const &);
	Action & operator=(Action const &);

	SwgCuiCombatQueue & m_mediator;
};

//----------------------------------------------------------------------

SwgCuiCombatQueue::SwgCuiCombatQueue(UIPage & page) :
CuiMediator("SwgCuiCombatQueue", page),
UIEventCallback(),
m_callback(new MessageDispatch::Callback),
m_action(new Action(*this)),
m_volumePage(0),
m_sampleItem(0),
m_clearButton(0),
m_peaceAttackButton(0),
m_targetNameText(0),
m_queuePages(new QueuePageMap),
m_removalTimers(new RemovalTimerMap),
m_lastCombatTarget(),
m_expanded(true)
{
	getCodeDataObject(TUIVolumePage, m_volumePage, "VolumePage");
	getCodeDataObject(TUIPage, m_sampleItem, "SampleItem");
	getCodeDataObject(TUIButton, m_clearButton, "ClearButton");
	getCodeDataObject(TUIButton, m_peaceAttackButton, "PeaceAttackButton");
	getCodeDataObject(TUIText, m_targetNameText, "TargetNameText");

	m_sampleItem->SetVisible(false);
	m_volumePage->Clear();
	m_targetNameText->Clear();

	CuiActionManager::addAction(SwgCuiActions::combatQueueCollapse, m_action, false);
	CuiActionManager::addAction(SwgCuiActions::combatQueueExpand, m_action, false);
	CuiActionManager::addAction(SwgCuiActions::clearCombatQueue, m_action, false);

	// These subscriptions deliberately live for the mediator's lifetime.  The
	// combat-state callback must be able to reactivate an inactive queue, and a
	// newly queued combat command must be observed while the page is hidden.
	m_callback->connect(*this, &SwgCuiCombatQueue::onCommandAdded, static_cast<ClientCommandQueue::Messages::Added *>(0));
	m_callback->connect(*this, &SwgCuiCombatQueue::onCommandRemoving, static_cast<ClientCommandQueue::Messages::Removing *>(0));
	m_callback->connect(*this, &SwgCuiCombatQueue::onCreatureStatesChanged, static_cast<CreatureObject::Messages::StatesChanged *>(0));
}

//----------------------------------------------------------------------

SwgCuiCombatQueue::~SwgCuiCombatQueue()
{
	deactivate();

	m_callback->disconnect(*this, &SwgCuiCombatQueue::onCommandAdded, static_cast<ClientCommandQueue::Messages::Added *>(0));
	m_callback->disconnect(*this, &SwgCuiCombatQueue::onCommandRemoving, static_cast<ClientCommandQueue::Messages::Removing *>(0));
	m_callback->disconnect(*this, &SwgCuiCombatQueue::onCreatureStatesChanged, static_cast<CreatureObject::Messages::StatesChanged *>(0));

	delete m_callback;
	m_callback = 0;

	delete m_action;
	m_action = 0;

	resetQueue();
	delete m_queuePages;
	m_queuePages = 0;
	delete m_removalTimers;
	m_removalTimers = 0;

	m_volumePage = 0;
	m_sampleItem = 0;
	m_clearButton = 0;
	m_peaceAttackButton = 0;
	m_targetNameText = 0;
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::performActivate()
{
	m_clearButton->AddCallback(this);
	m_peaceAttackButton->AddCallback(this);

	resetQueue();
	CuiCombatManager::IntVector sequenceIds;
	CuiCombatManager::getCombatCommandsFromQueue(sequenceIds);

	for (CuiCombatManager::IntVector::const_iterator it = sequenceIds.begin(); it != sequenceIds.end(); ++it)
	{
		ClientCommandQueue::Entry const * const entry = ClientCommandQueue::findEntry(*it);
		if (entry)
			addCommand(*it, *entry);
	}

	setIsUpdating(true);
	updateTarget();
	updateVisibility();
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::performDeactivate()
{
	m_clearButton->RemoveCallback(this);
	m_peaceAttackButton->RemoveCallback(this);
	m_removalTimers->clear();
	setIsUpdating(false);
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::OnButtonPressed(UIWidget * context)
{
	if (context == m_clearButton)
	{
		clearCombatQueue();
	}
	else if (context == m_peaceAttackButton)
	{
		IGNORE_RETURN(ClientCommandQueue::enqueueCommand("peace", NetworkId::cms_invalid, Unicode::emptyString));
	}
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);

	for (RemovalTimerMap::iterator it = m_removalTimers->begin(); it != m_removalTimers->end(); )
	{
		it->second -= deltaTimeSecs;
		if (it->second <= 0.0f)
		{
			uint32 const sequenceId = it->first;
			RemovalTimerMap::iterator const eraseIt = it++;
			m_removalTimers->erase(eraseIt);
			removeCommandPage(sequenceId);
		}
		else
		{
			++it;
		}
	}

	updateTarget();
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::clearCombatQueue()
{
	ClientCommandQueue::clear();
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::setExpanded(bool const expanded)
{
	if (m_expanded != expanded)
	{
		m_expanded = expanded;
		updateVisibility();
	}
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::updateTarget()
{
	CachedNetworkId const & target = CuiCombatManager::getCombatTarget();
	Object const * const object = target.getObject();

	// A network id can arrive before its client object.  Do not cache that
	// unresolved presentation or the target name would remain blank after the
	// object enters the client world.
	if (target.isValid() && !object)
	{
		m_lastCombatTarget = CachedNetworkId::cms_cachedInvalid;
		m_targetNameText->Clear();
		return;
	}

	if (target == m_lastCombatTarget)
		return;

	m_lastCombatTarget = target;
	Unicode::String targetName;

	ClientObject const * const clientObject = object ? object->asClientObject() : 0;
	if (clientObject)
		targetName = clientObject->getLocalizedName();

	m_targetNameText->SetLocalText(targetName);
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::addCommand(uint32 const sequenceId, ClientCommandQueue::Entry const & entry)
{
	if (!entry.m_command || !CuiCombatManager::isCombatQueueCommand(*entry.m_command))
		return;

	if (m_queuePages->find(sequenceId) != m_queuePages->end())
		return;

	UISize scrollExtent;
	m_volumePage->GetScrollExtent(scrollExtent);
	UIPoint const scrollLocation = m_volumePage->GetScrollLocation();
	bool const wasAtBottom = scrollLocation.y + m_volumePage->GetHeight() >= scrollExtent.y;

	UIPage * const commandPage = safe_cast<UIPage *>(m_sampleItem->DuplicateObject());
	NOT_NULL(commandPage);

	char pageName[32];
	sprintf(pageName, "%lu", static_cast<unsigned long>(sequenceId));
	commandPage->SetName(pageName);
	commandPage->SetEnabled(true);
	commandPage->SetVisible(true);
	commandPage->SetLocation(UIPoint::zero);

	UIImage * const icon = safe_cast<UIImage *>(commandPage->GetChild("Icon"));
	UIText * const nameText = safe_cast<UIText *>(commandPage->GetChild("NameText"));

	CuiDragInfo dragInfo;
	dragInfo.type = CuiDragInfoTypes::CDIT_command;
	dragInfo.str = std::string("/") + entry.m_command->m_commandName;
	UIImageStyle * const imageStyle = CuiIconManager::findIconImageStyle(dragInfo);
	if (imageStyle)
		icon->SetStyle(imageStyle);

	Unicode::String localizedCommandName;
	if (!CuiSkillManager::localizeCmdName(Unicode::toLower(entry.m_command->m_commandName), localizedCommandName))
		localizedCommandName = Unicode::narrowToWide(entry.m_command->m_commandName);
	nameText->SetLocalText(localizedCommandName);

	commandPage->Attach(0);
	IGNORE_RETURN(m_volumePage->AddChild(commandPage));
	commandPage->Link();
	(*m_queuePages)[sequenceId] = commandPage;

	if (wasAtBottom)
		m_volumePage->ScrollToBottom();
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::onCommandAdded(ClientCommandQueue::Messages::Added::Payload const & payload)
{
	ClientCommandQueue::Entry const * const entry = payload.second;
	if (!entry || !entry->m_command || !CuiCombatManager::isCombatQueueCommand(*entry->m_command))
		return;

	if (!isActive())
		activate();
	else
		addCommand(payload.first, *entry);
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::onCommandRemoving(ClientCommandQueue::Messages::Removing::Payload const & payload)
{
	QueuePageMap::iterator const row = m_queuePages->find(payload.sequenceId);
	if (row == m_queuePages->end())
		return;

	// The queue entry pointer becomes invalid as soon as this callback returns.
	// Retain only the sequence and wait time needed for the legacy fade/removal.
	row->second->SetEnabled(false);
	if (!isActive() || payload.waitTime <= 0.0f)
	{
		removeCommandPage(payload.sequenceId);
	}
	else
	{
		(*m_removalTimers)[payload.sequenceId] = payload.waitTime;
	}
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::onCreatureStatesChanged(CreatureObject::Messages::StatesChanged::Payload const & creature)
{
	if (Game::getPlayerCreature() == &creature)
		updateVisibility();
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::removeCommandPage(uint32 const sequenceId)
{
	QueuePageMap::iterator const it = m_queuePages->find(sequenceId);
	if (it == m_queuePages->end())
		return;

	UIPage * const page = it->second;
	IGNORE_RETURN(m_volumePage->RemoveChild(page));
	page->Detach(0);
	m_queuePages->erase(it);
	m_volumePage->Link();
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::resetQueue()
{
	if (!m_queuePages || !m_volumePage)
		return;

	for (QueuePageMap::iterator it = m_queuePages->begin(); it != m_queuePages->end(); ++it)
	{
		UIPage * const page = it->second;
		IGNORE_RETURN(m_volumePage->RemoveChild(page));
		page->Detach(0);
	}

	m_queuePages->clear();
	if (m_removalTimers)
		m_removalTimers->clear();
	m_volumePage->Link();
}

//----------------------------------------------------------------------

void SwgCuiCombatQueue::updateVisibility()
{
	CachedNetworkId combatTarget;
	bool const shouldBeActive = m_expanded && CuiCombatManager::isInCombat(Game::getPlayerCreature(), combatTarget);

	getPage().SetVisible(shouldBeActive);
	if (shouldBeActive)
	{
		if (!isActive())
			activate();
	}
	else if (isActive())
	{
		deactivate();
	}
}

//======================================================================
