//======================================================================
//
// SwgCuiBuffDisplay.cpp
// copyright (c) 2001 Sony Online Entertainment
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiBuffDisplay.h"

#include "clientGame/ClientBuffManager.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "clientGame/PlayerObject.h"
#include "sharedGame/Buff.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "swgClientUserInterface/SwgCuiBuffUtils.h"
#include "UIButton.h"
#include "UIEffector.h"
#include "UIImage.h"
#include "UIImageStyle.h"
#include "UIManager.h"
#include "UIPage.h"
#include "UIVolumePage.h"

//======================================================================

namespace SwgCuiBuffDisplayNamespace
{
	const float UPDATE_FREQUENCY = 0.25f;

	const int NUM_BUFFS_DEBUFFS = 10;
	
	static const UIColor s_debuffColor = UIColor(255, 0, 0);
	static const UIColor s_buffColor = UIColor(255, 0, 255);
	
	static const UILowerString BUFF_TIMESTAMP_PROPERTY = UILowerString("BuffTimestamp");
	static const int PLAYER_BUFF_BLINK_TIME = 10;

}

using namespace SwgCuiBuffDisplayNamespace;

SwgCuiBuffDisplay::SwgCuiBuffDisplay(UIPage & page) :
CuiMediator("SwgCuiBuffDisplay", page),
UIEventCallback(),
m_objectId(),
m_callback(new MessageDispatch::Callback),
m_volume(0),
m_blank(0),
m_sampleIcon(0),
m_closeButton(0),
m_internalTimer(0.0f),
m_effectorBlink(NULL),
m_dynamicIcons(false),
m_userClosed(false)
{
	getCodeDataObject (TUIVolumePage, m_volume, "volume", true);
	if (!m_volume)
		getCodeDataObject (TUIVolumePage, m_volume, "VolumePage", true);

	getCodeDataObject (TUIImageStyle, m_blank, "blank", true);	
	getCodeDataObject (TUIImage, m_sampleIcon, "sampleIcon", true);
	getCodeDataObject (TUIButton, m_closeButton, "buttonclose", true);
	getCodeDataObject (TUIEffector, m_effectorBlink,      "effectorBlink", true);

	m_dynamicIcons = (m_volume != 0 && m_sampleIcon != 0);
	if (m_sampleIcon)
		m_sampleIcon->SetVisible(false);

	if (m_dynamicIcons)
	{
		// Publish 14 authored the volume with dozens of sample state icons.
		// They are templates, not active effects, and must never survive page setup.
		m_volume->Clear();
		getPage().SetVisible(false);
	}

	if (m_closeButton)
		registerMediatorObject(*m_closeButton, true);
}

//----------------------------------------------------------------------

SwgCuiBuffDisplay::~SwgCuiBuffDisplay ()
{
	delete m_callback;
	m_callback = 0;

	if (m_dynamicIcons && m_volume)
		SwgCuiBuffUtils::clearBuffIcons(*m_volume);

	m_volume = 0;
	m_blank = 0;
	m_sampleIcon = 0;
	m_closeButton = 0;
}

// ----------------------------------------------------------------------

void SwgCuiBuffDisplay::performActivate()
{
	setIsUpdating(true);
	m_userClosed = false;
	m_internalTimer = UPDATE_FREQUENCY;
	update(0.0f);
}

//-----------------------------------------------------------------------------

void SwgCuiBuffDisplay::performDeactivate()
{
	setIsUpdating(false);
}

//-----------------------------------------------------------------------------

void SwgCuiBuffDisplay::update(float deltaTimeSecs)
{
	m_internalTimer += deltaTimeSecs;
	if (m_internalTimer < UPDATE_FREQUENCY)
		return;
	m_internalTimer = 0.0f;

	const CreatureObject * const creatureObject = dynamic_cast<const CreatureObject *>(m_objectId.getObject());
	if (!creatureObject)
	{
		if (m_dynamicIcons && m_volume)
		{
			SwgCuiBuffUtils::clearBuffIcons(*m_volume);
			getPage().SetVisible(false);
		}
		return;
	}

	if (m_dynamicIcons)
	{
		uint32 const state = SwgCuiBuffUtils::updateBuffs(
			*creatureObject,
			*m_volume,
			*m_volume,
			*m_sampleIcon,
			m_effectorBlink,
			0);
		bool const hasActiveEffects = (state & (SwgCuiBuffUtils::UBRT_hasBuffs | SwgCuiBuffUtils::UBRT_hasDebufs)) != 0;
		getPage().SetVisible(hasActiveEffects && !m_userClosed);
		m_volume->Pack();

		CreatureObject const * const playerCreature = Game::getPlayerCreature();
		if (playerCreature && m_objectId == playerCreature->getNetworkId())
		{
			ClientBuffManager::setStatusPanelDiagnostics(
				static_cast<uint32>(m_volume->GetChildCount()),
				getPage().IsVisible());
		}
		return;
	}

	if (!m_volume || !m_blank)
		return;

	std::vector<Buff> creatureBuffs;
	creatureObject->getBuffs(creatureBuffs);
	
	for (int j = 0; j < NUM_BUFFS_DEBUFFS * 2; j++)
	{
		UIWidget * wid = m_volume->FindCell(j);
		UIImage * img = dynamic_cast<UIImage *>(wid);
		if(!img)
			return;
		img->SetStyle(m_blank);
	}

	int curBuffIndex = -1;
	int curDebuffIndex = -1;
	for (std::vector<Buff>::const_iterator i = creatureBuffs.begin(); i != creatureBuffs.end(); ++i)
	{
		const Buff & buff = *i;
		if (!ClientBuffManager::getBuffIsGroupVisible(buff.m_nameCrc))
			continue;
		UIImageStyle * imageStyle = ClientBuffManager::getBuffIconStyle(buff.m_nameCrc);
		bool isDebuff = ClientBuffManager::getBuffIsDebuff(buff.m_nameCrc);
		int targetIndex = isDebuff ? (NUM_BUFFS_DEBUFFS + (++curDebuffIndex)) : (++curBuffIndex);
		if ((curBuffIndex >= NUM_BUFFS_DEBUFFS) || (curDebuffIndex >= NUM_BUFFS_DEBUFFS))
		{
			WARNING(true, ("Character has too many buffs or debuffs to display in the UI\n"));
			continue;
		}
		UIWidget * wid = m_volume->FindCell(targetIndex);
		UIImage * img = dynamic_cast<UIImage *>(wid);
		img->SetStyle(imageStyle);		
		
		int timeLeft = 0;
		if (buff.m_timestamp > creatureObject->getPlayedTime())
			timeLeft = buff.m_timestamp - creatureObject->getPlayedTime();
		
		Unicode::String tooltipStr;
		Unicode::String result;
		ClientBuffManager::getBuffDescription(buff, tooltipStr);
		if (timeLeft >= 0)
		{
			if (m_effectorBlink && (timeLeft <= PLAYER_BUFF_BLINK_TIME))
				UIManager::gUIManager().ExecuteEffector(m_effectorBlink, img, false);	
			ClientBuffManager::addTimestampToBuffDescription(tooltipStr, timeLeft, result);
		}
		img->SetLocalTooltip(result);
		
		if (imageStyle)
		{
			img->SetColor(UIColor::white);
		}
	}
		
}

// ----------------------------------------------------------------------

void SwgCuiBuffDisplay::OnButtonPressed(UIWidget * context)
{
	if (context == m_closeButton)
	{
		m_userClosed = true;
		getPage().SetVisible(false);
	}
}

// ----------------------------------------------------------------------

void SwgCuiBuffDisplay::setTarget(CreatureObject * creature)
{
	if (creature)
		setTarget(creature->getNetworkId());
	else
		setTarget(NetworkId::cms_invalid);
}
//-----------------------------------------------------------------------------

void SwgCuiBuffDisplay::setTarget(const NetworkId & id)
{
	if (id != m_objectId)
	{		
		m_objectId = id;
		m_userClosed = false;
		m_internalTimer = UPDATE_FREQUENCY;
		update(0.0f);
	}
}

//======================================================================
