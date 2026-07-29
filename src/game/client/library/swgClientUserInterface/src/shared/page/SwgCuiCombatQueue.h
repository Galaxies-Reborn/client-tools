//======================================================================
//
// SwgCuiCombatQueue.h
// copyright (c) 2002 Sony Online Entertainment
//
//======================================================================

#ifndef INCLUDED_SwgCuiCombatQueue_H
#define INCLUDED_SwgCuiCombatQueue_H

//======================================================================

#include "clientGame/ClientCommandQueue.h"
#include "clientGame/CreatureObject.h"
#include "clientUserInterface/CuiMediator.h"
#include "sharedObject/CachedNetworkId.h"
#include "UIEventCallback.h"

class UIButton;
class UIPage;
class UIText;
class UIVolumePage;

namespace MessageDispatch
{
	class Callback;
}

//----------------------------------------------------------------------

class SwgCuiCombatQueue :
public CuiMediator,
public UIEventCallback
{
public:

	class Action;

	explicit SwgCuiCombatQueue(UIPage & page);

	virtual void OnButtonPressed(UIWidget * context);
	virtual void update(float deltaTimeSecs);

	void clearCombatQueue();
	void setExpanded(bool expanded);
	void updateTarget();

protected:

	virtual void performActivate();
	virtual void performDeactivate();

private:

	typedef stdmap<uint32, UIPage *>::fwd QueuePageMap;
	typedef stdmap<uint32, float>::fwd RemovalTimerMap;

	virtual ~SwgCuiCombatQueue();
	SwgCuiCombatQueue();
	SwgCuiCombatQueue(SwgCuiCombatQueue const &);
	SwgCuiCombatQueue & operator=(SwgCuiCombatQueue const &);

	void addCommand(uint32 sequenceId, ClientCommandQueue::Entry const & entry);
	void onCommandAdded(ClientCommandQueue::Messages::Added::Payload const & payload);
	void onCommandRemoving(ClientCommandQueue::Messages::Removing::Payload const & payload);
	void onCreatureStatesChanged(CreatureObject::Messages::StatesChanged::Payload const & creature);
	void removeCommandPage(uint32 sequenceId);
	void resetQueue();
	void updateVisibility();

	MessageDispatch::Callback * m_callback;
	Action *                    m_action;
	UIVolumePage *              m_volumePage;
	UIPage *                    m_sampleItem;
	UIButton *                  m_clearButton;
	UIButton *                  m_peaceAttackButton;
	UIText *                    m_targetNameText;
	QueuePageMap *              m_queuePages;
	RemovalTimerMap *           m_removalTimers;
	CachedNetworkId             m_lastCombatTarget;
	bool                        m_expanded;
};

//======================================================================

#endif
