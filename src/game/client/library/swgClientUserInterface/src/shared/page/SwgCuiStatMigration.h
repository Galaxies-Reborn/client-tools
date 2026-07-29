// ======================================================================
//
// SwgCuiStatMigration.h
// Publish 14 nine-attribute stat migration mediator.
//
// ======================================================================

#ifndef INCLUDED_SwgCuiStatMigration_H
#define INCLUDED_SwgCuiStatMigration_H

#include "UIEventCallback.h"
#include "clientUserInterface/CuiMediator.h"
#include "sharedMessageDispatch/Receiver.h"

#include <utility>
#include <vector>

class CreatureObject;
class UIButton;
class UIPage;
class UISliderbar;
class UIText;

namespace MessageDispatch
{
	class Callback;
}

template <typename T> class Watcher;

class SwgCuiStatMigration :
public CuiMediator,
public UIEventCallback,
public MessageDispatch::Receiver
{
public:
	explicit SwgCuiStatMigration(UIPage & page);

	virtual void OnButtonPressed(UIWidget * context);
	virtual void OnSliderbarChanged(UIWidget * context);
	virtual void receiveMessage(MessageDispatch::Emitter const & source, MessageDispatch::MessageBase const & message);

	void onMaxAttributesChanged(CreatureObject const & creature);

protected:
	virtual void performActivate();
	virtual void performDeactivate();

private:
	~SwgCuiStatMigration();
	SwgCuiStatMigration(SwgCuiStatMigration const &);
	SwgCuiStatMigration & operator=(SwgCuiStatMigration const &);

	void setMigrationControlsEnabled(bool enabled);
	void updatePointsLeftText();
	void updateAttributeDisplay(int index);
	void updateAllAttributeDisplays();
	void sendTargets();

private:
	UIButton * m_buttonCancel;
	UIButton * m_buttonOk;
	UIText * m_pointsLeftText;
	UIPage * m_statPages[9];
	UISliderbar * m_sliders[9];
	UIText * m_currentTexts[9];
	UIText * m_targetTexts[9];
	std::vector<int> m_current;
	std::vector<int> m_targets;
	std::vector<std::pair<int, int> > m_minMaxes;
	int m_pointsLeft;
	bool m_receivedTargets;
	bool m_callbacksConnected;
	MessageDispatch::Callback * m_callback;
	typedef Watcher<CreatureObject> CreatureWatcher;
	CreatureWatcher * m_playerWatcher;
};

#endif
