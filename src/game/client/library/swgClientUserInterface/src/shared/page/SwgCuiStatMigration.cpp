// ======================================================================
//
// SwgCuiStatMigration.cpp
// Publish 14 nine-attribute stat migration mediator.
//
// The UI and network contracts follow the 14.1 client: nine ordered sliders,
// a server-owned points-left value, and a ten-integer set-command payload.
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiStatMigration.h"

#include "UIButton.h"
#include "UIData.h"
#include "UIPage.h"
#include "UISliderbar.h"
#include "UIText.h"

#include "clientGame/ClientCommandQueue.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiMessageBox.h"
#include "clientUserInterface/CuiStringIdsCharacterSheet.h"
#include "clientUserInterface/CuiStringVariablesData.h"
#include "clientUserInterface/CuiStringVariablesManager.h"
#include "sharedFoundation/Watcher.h"
#include "sharedGame/PlayerCreationManager.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedNetworkMessages/GameNetworkMessage.h"
#include "sharedNetworkMessages/StatMigrationTargetsMessage.h"
#include "swgSharedUtility/Attributes.h"

#include <algorithm>
#include <cstdio>

namespace SwgCuiStatMigrationNamespace
{
	int const s_attributeCount = 9;

	char const * const s_statPageCodeDataNames[s_attributeCount] =
	{
		"statsHealth",
		"statsStrength",
		"statsConstitution",
		"statsAction",
		"statsQuickness",
		"statsStamina",
		"statsMind",
		"statsFocus",
		"statsWillPower"
	};

	int const s_attributes[s_attributeCount] =
	{
		Attributes::Health,
		Attributes::Strength,
		Attributes::Constitution,
		Attributes::Action,
		Attributes::Quickness,
		Attributes::Stamina,
		Attributes::Mind,
		Attributes::Focus,
		Attributes::Willpower
	};

	Unicode::String formatStatText(char const * key, int value)
	{
		CuiStringVariablesData data;
		data.digit_i = value;
		Unicode::String result;
		CuiStringVariablesManager::process(StringId("ui", key), data, result);
		return result;
	}
}

using namespace SwgCuiStatMigrationNamespace;

SwgCuiStatMigration::SwgCuiStatMigration(UIPage & page)
:
CuiMediator("SwgCuiStatMigration", page),
UIEventCallback(),
MessageDispatch::Receiver(),
m_buttonCancel(0),
m_buttonOk(0),
m_pointsLeftText(0),
m_current(s_attributeCount, 0),
m_targets(s_attributeCount, 0),
m_minMaxes(s_attributeCount, std::make_pair(0, 0)),
m_pointsLeft(0),
m_receivedTargets(false),
m_callbacksConnected(false),
m_callback(new MessageDispatch::Callback),
m_playerWatcher(new CreatureWatcher)
{
	setState(MS_closeable);
	setState(MS_closeDeactivates);
	removeState(MS_iconifiable);

	getCodeDataObject(TUIButton, m_buttonCancel, "buttonCancel");
	getCodeDataObject(TUIButton, m_buttonOk, "buttonOk");
	getCodeDataObject(TUIText, m_pointsLeftText, "pointsLeft");
	registerMediatorObject(*m_buttonCancel, true);
	registerMediatorObject(*m_buttonOk, true);

	for (int i = 0; i < s_attributeCount; ++i)
	{
		m_statPages[i] = 0;
		m_sliders[i] = 0;
		m_currentTexts[i] = 0;
		m_targetTexts[i] = 0;

		getCodeDataObject(TUIPage, m_statPages[i], s_statPageCodeDataNames[i]);
		UIData const * const codeData = dynamic_cast<UIData const *>(m_statPages[i]->GetChild("CodeData"));
		DEBUG_FATAL(!codeData, ("Publish 14 stat migration page '%s' is missing CodeData", s_statPageCodeDataNames[i]));
		getCodeDataObject(m_statPages[i], codeData, TUISliderbar, m_sliders[i], "stat");
		getCodeDataObject(m_statPages[i], codeData, TUIText, m_currentTexts[i], "current");
		getCodeDataObject(m_statPages[i], codeData, TUIText, m_targetTexts[i], "target");
		registerMediatorObject(*m_sliders[i], true);
	}

	setMigrationControlsEnabled(false);
}

SwgCuiStatMigration::~SwgCuiStatMigration()
{
	delete m_callback;
	m_callback = 0;
	delete m_playerWatcher;
	m_playerWatcher = 0;
}

void SwgCuiStatMigration::performActivate()
{
	CreatureObject * const player = Game::getPlayerCreature();
	if (!player || !player->getObjectTemplateName())
	{
		closeNextFrame();
		return;
	}

	*m_playerWatcher = player;
	m_receivedTargets = false;
	setMigrationControlsEnabled(false);

	std::string const templateName(player->getObjectTemplateName());
	if (!PlayerCreationManager::getRacialMinMaxes(templateName, m_minMaxes) ||
		static_cast<int>(m_minMaxes.size()) != s_attributeCount)
	{
		WARNING(true, ("No Publish 14 attribute limits for player template '%s'", templateName.c_str()));
		closeNextFrame();
		return;
	}

	for (int i = 0; i < s_attributeCount; ++i)
	{
		m_current[i] = player->getUnmodifiedMaxAttribute(static_cast<Attributes::Enumerator>(s_attributes[i]));
		m_targets[i] = m_current[i];
		m_sliders[i]->SetLowerLimit(m_minMaxes[i].first);
		m_sliders[i]->SetUpperLimit(m_minMaxes[i].second);
		m_sliders[i]->SetValue(m_targets[i], false);
	}
	m_pointsLeft = 0;
	updateAllAttributeDisplays();

	connectToMessage(StatMigrationTargetsMessage::cms_name);
	m_callback->connect(*this, &SwgCuiStatMigration::onMaxAttributesChanged,
		static_cast<CreatureObject::Messages::MaxAttributesChanged *>(0));
	m_callbacksConnected = true;

	ClientCommandQueue::enqueueCommand("requestStatMigrationData", NetworkId::cms_invalid, Unicode::emptyString);
	CuiManager::requestPointer(true);
}

void SwgCuiStatMigration::performDeactivate()
{
	if (m_callbacksConnected)
	{
		disconnectFromMessage(StatMigrationTargetsMessage::cms_name);
		m_callback->disconnect(*this, &SwgCuiStatMigration::onMaxAttributesChanged,
			static_cast<CreatureObject::Messages::MaxAttributesChanged *>(0));
		m_callbacksConnected = false;
	}

	*m_playerWatcher = static_cast<CreatureObject *>(0);
	m_receivedTargets = false;
	setMigrationControlsEnabled(false);
	CuiManager::requestPointer(false);
}

void SwgCuiStatMigration::OnButtonPressed(UIWidget * context)
{
	if (context == m_buttonCancel)
	{
		deactivate();
		return;
	}

	if (context == m_buttonOk && m_receivedTargets)
	{
		if (m_pointsLeft != 0)
		{
			CuiMessageBox::createInfoBox(CuiStringIdsCharacterSheet::statmig_usealltpoints.localize());
			return;
		}

		sendTargets();
		deactivate();
	}
}

void SwgCuiStatMigration::OnSliderbarChanged(UIWidget * context)
{
	if (!m_receivedTargets)
		return;

	for (int i = 0; i < s_attributeCount; ++i)
	{
		if (context != m_sliders[i])
			continue;

		int const oldTarget = m_targets[i];
		int requestedTarget = m_sliders[i]->GetValue();
		int delta = requestedTarget - oldTarget;
		if (delta > m_pointsLeft)
		{
			delta = m_pointsLeft;
			requestedTarget = oldTarget + delta;
			m_sliders[i]->SetValue(requestedTarget, false);
		}

		if (m_pointsLeft - delta < 0)
		{
			m_sliders[i]->SetValue(oldTarget, false);
			return;
		}

		m_targets[i] = requestedTarget;
		m_pointsLeft -= delta;
		updateAttributeDisplay(i);
		updatePointsLeftText();
		return;
	}
}

void SwgCuiStatMigration::receiveMessage(MessageDispatch::Emitter const &, MessageDispatch::MessageBase const & message)
{
	if (!message.isType(StatMigrationTargetsMessage::cms_name))
		return;

	Archive::ReadIterator reader = NON_NULL(safe_cast<GameNetworkMessage const *>(&message))->getByteStream().begin();
	StatMigrationTargetsMessage const targetsMessage(reader);
	std::vector<int> const targets = targetsMessage.getTargets();
	if (static_cast<int>(targets.size()) != s_attributeCount)
	{
		WARNING(true, ("StatMigrationTargetsMessage contained %u targets; expected %d",
			static_cast<unsigned int>(targets.size()), s_attributeCount));
		return;
	}

	m_targets = targets;
	m_pointsLeft = std::max(0, targetsMessage.getPointsLeft());
	for (int i = 0; i < s_attributeCount; ++i)
		m_sliders[i]->SetValue(m_targets[i], false);

	m_receivedTargets = true;
	setMigrationControlsEnabled(true);
	updateAllAttributeDisplays();
}

void SwgCuiStatMigration::onMaxAttributesChanged(CreatureObject const & creature)
{
	CreatureObject const * const player = m_playerWatcher->getPointer();
	if (!player || player != &creature)
		return;

	for (int i = 0; i < s_attributeCount; ++i)
		m_current[i] = creature.getUnmodifiedMaxAttribute(static_cast<Attributes::Enumerator>(s_attributes[i]));
	updateAllAttributeDisplays();
}

void SwgCuiStatMigration::setMigrationControlsEnabled(bool enabled)
{
	if (m_buttonOk)
		m_buttonOk->SetEnabled(enabled);
	for (int i = 0; i < s_attributeCount; ++i)
		if (m_sliders[i])
			m_sliders[i]->SetEnabled(enabled);
}

void SwgCuiStatMigration::updatePointsLeftText()
{
	if (m_pointsLeftText)
		m_pointsLeftText->SetLocalText(formatStatText("stat_pointsleft", m_pointsLeft));
}

void SwgCuiStatMigration::updateAttributeDisplay(int index)
{
	if (index < 0 || index >= s_attributeCount)
		return;
	if (m_currentTexts[index])
		m_currentTexts[index]->SetLocalText(formatStatText("stat_current", m_current[index]));
	if (m_targetTexts[index])
		m_targetTexts[index]->SetLocalText(formatStatText("stat_target", m_targets[index]));
}

void SwgCuiStatMigration::updateAllAttributeDisplays()
{
	for (int i = 0; i < s_attributeCount; ++i)
		updateAttributeDisplay(i);
	updatePointsLeftText();
}

void SwgCuiStatMigration::sendTargets()
{
	char buffer[256];
	_snprintf(buffer, sizeof(buffer), "%d %d %d %d %d %d %d %d %d %d",
		m_targets[0], m_targets[1], m_targets[2],
		m_targets[3], m_targets[4], m_targets[5],
		m_targets[6], m_targets[7], m_targets[8], m_pointsLeft);
	buffer[sizeof(buffer) - 1] = 0;
	ClientCommandQueue::enqueueCommand("requestSetStatMigrationData", NetworkId::cms_invalid,
		Unicode::narrowToWide(buffer));
}
