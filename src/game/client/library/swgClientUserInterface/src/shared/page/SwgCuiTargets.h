//======================================================================
//
// SwgCuiTargets.h
// copyright (c) 2001 Sony Online Entertainment
//
//======================================================================

#ifndef INCLUDED_SwgCuiTargets_H
#define INCLUDED_SwgCuiTargets_H

//======================================================================

#include "clientUserInterface/CuiMediator.h"
#include "sharedObject/CachedNetworkId.h"

#include "UIEventCallback.h"

//----------------------------------------------------------------------

class UIPage;
class UIScrollbar;
class UIVolumePage;
class SwgCuiMfdStatus;
class UIButton;

//----------------------------------------------------------------------

class SwgCuiTargets :
public CuiMediator,
public UIEventCallback
{
public:
	enum TargetRole
	{
		TR_primary,
		TR_targetOfTarget
	};

	explicit            SwgCuiTargets (UIPage & page, TargetRole targetRole = TR_primary);

	void                update         (float deltaTimeSecs);
	const CachedNetworkId & getTarget   () const;
	void                clearTarget     ();

	void                OnButtonPressed (UIWidget *context);

protected:
	void                performActivate ();
	void                performDeactivate ();

private:
	                   ~SwgCuiTargets ();

	                    SwgCuiTargets ();
	                    SwgCuiTargets (const SwgCuiTargets &);
	SwgCuiTargets &     operator= (const SwgCuiTargets &);

private:	
	class SwgCuiTargetsAction;
	SwgCuiTargetsAction *  m_action;

	SwgCuiMfdStatus *      m_mfdStatus;

	UIButton *             m_buttonCollapse;
	UIButton *             m_buttonExpand;

	UIPage *               m_pageToggle;

	int m_sceneType; // Game::SceneType
	TargetRole m_targetRole;
};

//======================================================================

#endif
