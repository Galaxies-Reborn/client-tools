//======================================================================
//
// SwgCuiAppearanceTab.cpp
// copyright(c) 2008 Sony Online Entertainment
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiAppearanceTab.h"


#include "clientGame/ClientObject.h"
#include "clientGame/ClientObjectUsabilityManager.h"
#include "clientGame/ContainerInterface.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"

#include "clientSkeletalAnimation/SkeletalAppearance2.h"

#include "clientUserInterface/CuiDragInfo.h"
#include "clientUserInterface/CuiInventoryManager.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "clientUserInterface/CuiMediator.h"
#include "clientUserInterface/CuiPreferences.h"
#include "clientUserInterface/CuiStringIds.h"
#include "clientUserInterface/CuiSystemMessageManager.h"
#include "clientUserInterface/CuiWidget3dObjectListViewer.h"

#include "sharedFoundation/CrcLowerString.h"
#include "sharedFoundation/ConstCharCrcLowerString.h"
#include "sharedFoundation/ConstCharCrcString.h"

#include "sharedGame/GameObjectTypes.h"
#include "sharedGame/SharedObjectTemplate.h"
#include "sharedGame/WearableEntry.h"

#include "sharedObject/Appearance.h"
#include "sharedObject/Container.h"
#include "sharedObject/ObjectTemplateList.h"
#include "sharedObject/SlotId.h"
#include "sharedObject/SlotIdManager.h"
#include "sharedObject/SlottedContainer.h"
#include "sharedObject/SlottedContainmentProperty.h"

#include "swgClientUserInterface/SwgCuiAvatarCreationHelper.h"
#include "swgClientUserInterface/SwgCuiMediatorTypes.h"

#include "StringId.h"

#include "UIButton.h"
#include "UICheckbox.h"
#include "UIComposite.h"
#include "UIData.h"
#include "UIMessage.h"
#include "UIPage.h"
#include "UIText.h"
#include "UITypes.h"
#include "UIWidget.h"

#include "UnicodeUtils.h"

#include <vector>

namespace SwgCuiAppearanceTabNamespace
{
	namespace Properties
	{
		const UILowerString DefaultViewerPitch = UILowerString ("DefaultViewerPitch");
		const UILowerString DefaultViewerYaw   = UILowerString ("DefaultViewerYaw");
	}

	// This order must correspond to the order of the pages in LayoutPage.
	const char* ms_slotNames[] = 
	{
		"hat",
		"eyes",
		"bicep_r",
		"bicep_l",
		"chest2",
		"chest1",
		"bracer_upper_r",
		"bracer_lower_r",
		"bracer_upper_l",
		"bracer_lower_l",
		"gloves",
		"pants1",
		"shoes",
		"neck",
		"wrist_r",
		"wrist_l",
		"utility_belt",
		"back",
		"ring_r",
		"ring_l",
		"hold_l",
		"hold_r",
		"mouth",
		"cybernetic_hand_l",
		"cybernetic_hand_r"
	};

	unsigned int ms_totalSlotNames = sizeof(ms_slotNames)/sizeof(ms_slotNames[0]);
	
	const char * ms_defaultPants = "pants1";
	const char * ms_alternatePants = "pants2";

	const char * ms_slotNameSTFFile = "ui_appearance";
	std::vector<StringId> ms_slotDisplayNames;
	std::vector<CrcLowerString> ms_slotStringCrcs;

	CreatureObject * ms_dupedCreatureNoAppearance = NULL;

	void CleanupDupedCreature()
	{
		if(ms_dupedCreatureNoAppearance)
		{
			SlottedContainer * const container = ContainerInterface::getSlottedContainer(*ms_dupedCreatureNoAppearance);
			if(container)
			{
				for (ContainerIterator containerIterator = container->begin(); containerIterator != container->end(); ++containerIterator)
				{
					CachedNetworkId const & id = *containerIterator;
					if(id.getObject() == NULL)
						continue;
					ClientObject * const obj = id.getObject ()->asClientObject();
					if(obj)
						delete obj;
				}
			}
			delete ms_dupedCreatureNoAppearance;
			ms_dupedCreatureNoAppearance = NULL;
		}
	}

	UIColor defaultBGColor = UIColor(0, 0, 255);

	std::vector<CrcLowerString> ms_dragObjReqSlots;
	Object *                    ms_currentDragObject;
	StringId                    ms_cybernetics_error("ui_appearance", "cybernetics_error");
}

using namespace SwgCuiAppearanceTabNamespace;

// ---------------------------------------------------------------------

SwgCuiAppearanceTab::SwgCuiAppearanceTab (UIPage & page, bool embeddedInCharacterSheet):
CuiMediator            ("AppearanceTab", page),
m_characterViewer (NULL),
m_characterName (NULL),
m_closeButton (NULL),
m_showInventoryItems (NULL),
m_viewerPage (NULL),
m_layoutPage (NULL),
m_embeddedInCharacterSheet(embeddedInCharacterSheet),
m_embeddedWidth(0),
m_embeddedHeight(0),
m_lastDragItem( new CuiDragInfo ),
m_dragTimer(1.0f)
{
	if(ms_slotDisplayNames.empty())
	{
		for(unsigned int i = 0; i < ms_totalSlotNames; ++i)
		{
			StringId slotName(ms_slotNameSTFFile, ms_slotNames[i]);
			ms_slotDisplayNames.push_back(slotName);
		}
	}

	if(ms_slotStringCrcs.empty())
	{
		for(unsigned int i = 0; i < ms_totalSlotNames; ++i)
		{
			ms_slotStringCrcs.push_back(CrcLowerString(ms_slotNames[i]));
		}
	}


	UIWidget * viewer = 0;
	getCodeDataObject (TUIWidget,     viewer,            "viewer");
	registerMediatorObject(*viewer, true);

	m_characterViewer = NON_NULL (dynamic_cast<CuiWidget3dObjectListViewer *>(viewer));
	m_characterViewer->SetLocalTooltip (CuiStringIds::tooltip_viewer_3d_controls.localize ());
	m_characterViewer->SetPropertyFloat (Properties::DefaultViewerPitch, m_characterViewer->getCameraPitch ());
	m_characterViewer->SetPropertyFloat (Properties::DefaultViewerYaw,   m_characterViewer->getCameraYaw   ());

	m_characterViewer->setCameraLodBias (2.0f);
	m_characterViewer->setAutoZoomOutOnly       (false);
	m_characterViewer->setCameraZoomInWhileTurn (true);
	m_characterViewer->setAlterObjects          (false);
	m_characterViewer->setCameraLookAtCenter    (false);
	m_characterViewer->setDragYawOk             (true);
	m_characterViewer->setDragPitchOk           (true);
	m_characterViewer->setPaused                (false);
	m_characterViewer->SetDragable              (false);	
	m_characterViewer->SetContextCapable        (true, false);
	m_characterViewer->setRotateSpeed           (0.0f);
	m_characterViewer->setCameraForceTarget     (false);
	m_characterViewer->setCameraTransformToObj  (true);
	m_characterViewer->setCameraLodBias         (3.0f);
	m_characterViewer->setCameraLodBiasOverride (true);
	m_characterViewer->setCameraForceTarget   (true);
	if (Game::getClientPlayer())
	{
		m_characterViewer->setObject            (Game::getClientPlayer());
		m_characterViewer->recomputeZoom        ();
	}
	m_characterViewer->setRotationSlowsToStop (true);

	m_characterViewer->setCameraForceTarget   (false);
	m_characterViewer->setCameraAutoZoom      (true);
	m_characterViewer->setCameraLookAtBone    ("root");
	m_characterViewer->setCameraZoomLookAtBone("head");


	m_slotViewers.clear();
	m_slotText.clear();
	
	{
		getCodeDataObject(TUIPage, m_layoutPage, "layout");

		const UIBaseObject::UIObjectList & olist = m_layoutPage->GetChildrenRef ();
		
		for (UIBaseObject::UIObjectList::const_iterator it = olist.begin (); it != olist.end (); ++it)
		{
			UIPage * const p = safe_cast<UIPage *>(*it);
			NOT_NULL (p);

			const UIData * const slotCodeData = safe_cast<UIData *>(p->GetChild ("CodeData"));
			NOT_NULL (slotCodeData);

			static const UILowerString prop_text        ("text");
			static const UILowerString prop_viewer      ("viewer");

			std::string path_text;
			std::string path_viewer;

			if (!slotCodeData->GetPropertyNarrow (prop_text,   path_text))
				DEBUG_FATAL (true, ("SwgCuiAppearanceTab viewer page prop [%s] not found", prop_text.c_str ()));
			if (!slotCodeData->GetPropertyNarrow (prop_viewer, path_viewer))
				DEBUG_FATAL (true, ("SwgCuiAppearanceTab viewer page prop [%s] not found", prop_viewer.c_str ()));


			CuiWidget3dObjectListViewer * const objectListViewer = safe_cast<CuiWidget3dObjectListViewer *>(p->GetObjectFromPath (path_viewer.c_str (), TUIWidget));
			UIText * const text                        = safe_cast<UIText *>(p->GetObjectFromPath (path_text.c_str (), TUIText));

			DEBUG_FATAL (!objectListViewer,      ("SwgCuiAppearanceTab viewer not found [%s]",      path_viewer.c_str      ()));
			DEBUG_FATAL (!text,        ("SwgCuiAppearanceTab text not found [%s]",        path_text.c_str        ()));

			objectListViewer->setCameraLookAtCenter (true);
			objectListViewer->setPaused             (false);
			objectListViewer->setCameraForceTarget  (true);
			objectListViewer->setCameraLodBias      (3.0f);
			objectListViewer->setCameraLodBiasOverride(true);

			text->SetPreLocalized        (true);

			m_slotViewers.push_back      (objectListViewer);
			m_slotText.push_back        (text);

			registerMediatorObject(*objectListViewer, true);
		}	
	}

	getCodeDataObject(TUIButton, m_closeButton, "close");
	registerMediatorObject(*m_closeButton, true);

	getCodeDataObject(TUIText, m_characterName, "characterLabel");
	m_characterName->SetPreLocalized(true);

	getCodeDataObject(TUICheckbox, m_showInventoryItems, "checkShowInventory");
	registerMediatorObject(*m_showInventoryItems, true);
	m_showInventoryItems->SetChecked(false, false);

	if(m_slotViewers.size() > 0) // This should never be empty, but just incase.
		defaultBGColor = m_slotViewers[0]->GetBackgroundTint();

	registerMediatorObject(getPage(), true);

	getCodeDataObject(TUIPage, m_viewerPage, "viewerpage");
	registerMediatorObject(*m_viewerPage, true);

	if (m_embeddedInCharacterSheet)
	{
		getPage().SetUserMovable(false);
		getPage().SetMinimumSize(UISize::zero);
		getPage().SetMaximumSize(UISize(16000, 16000));

		UIWidget * const background = safe_cast<UIWidget *>(getPage().GetChild("bg"));
		background->SetVisible(false);
		background->SetEnabled(false);
		m_closeButton->SetVisible(false);
		m_closeButton->SetEnabled(false);

		UIWidget * const originalRunner = safe_cast<UIWidget *>(m_viewerPage->GetChild("original"));
		originalRunner->SetVisible(false);
		originalRunner->SetEnabled(false);
		const UIBaseObject::UIObjectList & viewerChildren = m_viewerPage->GetChildrenRef();
		for (UIBaseObject::UIObjectList::const_iterator it = viewerChildren.begin(); it != viewerChildren.end(); ++it)
		{
			UIWidget * const child = dynamic_cast<UIWidget *>(*it);
			if (child && child->GetName() == "box")
			{
				child->SetVisible(false);
				child->SetEnabled(false);
			}
		}

		layoutForEmbeddedParent();
	}
}

void SwgCuiAppearanceTab::layoutForEmbeddedParent()
{
	if (!m_embeddedInCharacterSheet)
		return;

	UIPage * const host = dynamic_cast<UIPage *>(getPage().GetParent());
	if (!host)
		return;

	UISize const hostSize = host->GetSize();
	if (hostSize.x == m_embeddedWidth && hostSize.y == m_embeddedHeight)
		return;

	m_embeddedWidth = hostSize.x;
	m_embeddedHeight = hostSize.y;
	// The Publish 14 sheet never reaches these floors, but keep every nested
	// preview and loading widget positive if a custom skin supplies a tiny host.
	long const width = hostSize.x < 280 ? 280 : hostSize.x;
	long const height = hostSize.y < 252 ? 252 : hostSize.y;
	long const slotWidth = 62;
	long const slotHeight = height / 7;
	long const lineHeight = slotHeight - 33;
	long const viewerLeft = slotWidth * 2;
	long const viewerWidth = width - (slotWidth * 4);

	getPage().SetLocation(UIPoint::zero);
	getPage().SetSize(UISize(width, height));
	getPage().SetScrollExtent(UISize(width, height));
	m_layoutPage->SetLocation(UIPoint::zero);
	m_layoutPage->SetSize(UISize(width, height));
	m_layoutPage->SetScrollExtent(UISize(width, height));

	const UIBaseObject::UIObjectList & slots = m_layoutPage->GetChildrenRef();
	size_t slotIndex = 0;
	for (UIBaseObject::UIObjectList::const_iterator it = slots.begin(); it != slots.end(); ++it, ++slotIndex)
	{
		UIPage * const slot = safe_cast<UIPage *>(*it);
		long column = 0;
		long row = 0;
		if (slotIndex < 7)
		{
			column = 0;
			row = static_cast<long>(slotIndex);
		}
		else if (slotIndex < 13)
		{
			column = slotWidth;
			row = static_cast<long>(slotIndex - 7);
		}
		else if (slotIndex < 19)
		{
			column = width - (slotWidth * 2);
			row = static_cast<long>(slotIndex - 13);
		}
		else
		{
			column = width - slotWidth;
			row = static_cast<long>(slotIndex - 19);
		}

		slot->SetMinimumSize(UISize::zero);
		slot->SetMaximumSize(UISize(16000, 16000));
		slot->SetLocation(UIPoint(column, row * slotHeight));
		slot->SetSize(UISize(slotWidth, slotHeight));
		slot->SetScrollExtent(UISize(slotWidth, slotHeight));

		UIText * const text = m_slotText[slotIndex];
		text->SetMinimumSize(UISize::zero);
		text->SetLocation(UIPoint(1, 0));
		text->SetSize(UISize(slotWidth - 2, 31));
		text->SetScrollExtent(UISize(slotWidth - 2, 31));

		UIPage * const line = safe_cast<UIPage *>(slot->GetChild("line"));
		line->SetMinimumSize(UISize::zero);
		line->SetMaximumSize(UISize(16000, 16000));
		line->SetLocation(UIPoint(2, 31));
		line->SetSize(UISize(slotWidth - 4, lineHeight));
		line->SetScrollExtent(UISize(slotWidth - 4, lineHeight));

		CuiWidget3dObjectListViewer * const slotViewer = m_slotViewers[slotIndex];
		slotViewer->SetLocation(UIPoint(1, 1));
		slotViewer->SetSize(UISize(slotWidth - 6, lineHeight - 2));
		slotViewer->SetScrollExtent(UISize(slotWidth - 6, lineHeight - 2));

		UIPage * const loading = safe_cast<UIPage *>(line->GetChild("loading"));
		loading->SetLocation(UIPoint::zero);
		loading->SetSize(UISize(slotWidth - 4, lineHeight));
		loading->SetScrollExtent(UISize(slotWidth - 4, lineHeight));
		UIWidget * const loadingText = safe_cast<UIWidget *>(loading->GetChild("text"));
		loadingText->SetLocation(UIPoint::zero);
		loadingText->SetSize(UISize(slotWidth - 4, lineHeight));
		loadingText->SetScrollExtent(UISize(slotWidth - 4, lineHeight));
		UIWidget * const loadingRunner = safe_cast<UIWidget *>(loading->GetChild("runner"));
		loadingRunner->SetVisible(false);
		loadingRunner->SetEnabled(false);
	}

	m_viewerPage->SetMinimumSize(UISize::zero);
	m_viewerPage->SetMaximumSize(UISize(16000, 16000));
	m_viewerPage->SetLocation(UIPoint(viewerLeft, 0));
	m_viewerPage->SetSize(UISize(viewerWidth, height));
	m_viewerPage->SetScrollExtent(UISize(viewerWidth, height));
	m_characterViewer->SetLocation(UIPoint(8, 24));
	m_characterViewer->SetSize(UISize(viewerWidth - 16, height - 54));
	m_characterViewer->SetScrollExtent(UISize(viewerWidth - 16, height - 54));
	m_characterName->SetMinimumSize(UISize::zero);
	m_characterName->SetLocation(UIPoint(8, 4));
	m_characterName->SetSize(UISize(viewerWidth - 16, 19));
	m_characterName->SetScrollExtent(UISize(viewerWidth - 16, 19));
	m_showInventoryItems->SetLocation(UIPoint(8, height - 26));
	m_showInventoryItems->SetSize(UISize(viewerWidth - 16, 19));
	m_showInventoryItems->SetScrollExtent(UISize(viewerWidth - 16, 19));
}

SwgCuiAppearanceTab::~SwgCuiAppearanceTab()
{
	if(ms_dupedCreatureNoAppearance)
		CleanupDupedCreature();

	ms_dupedCreatureNoAppearance = NULL;

	ms_slotStringCrcs.clear();

	ms_slotDisplayNames.clear();

	if(m_lastDragItem)
		delete m_lastDragItem;

	ms_dragObjReqSlots.clear();
	ms_currentDragObject = NULL;
}

void SwgCuiAppearanceTab::update(float delta)
{
	UNREF(delta);
	layoutForEmbeddedParent();

	CreatureObject * playerCreature = Game::getPlayerCreature();

	if(!playerCreature)
		return;

	if(!playerCreature->getAppearanceInventoryObject())
		return;

	m_characterName->SetLocalText(playerCreature->getLocalizedName());

	Object * itemContainer = m_showInventoryItems->IsChecked() ? playerCreature : playerCreature->getAppearanceInventoryObject();

	if(!itemContainer)
		return;

	SlottedContainer * slotContainer = ContainerInterface::getSlottedContainer(*itemContainer);

	if(!slotContainer)
		return;
	
	
	if(m_lastDragItem)
	{
		ClientObject * clientDragObj = m_lastDragItem->getClientObject();
		if(clientDragObj && ContainerInterface::getContainedByObject(*clientDragObj) != playerCreature->getAppearanceInventoryObject() && (clientDragObj != ms_currentDragObject || ms_dragObjReqSlots.empty() ) )
		{
			ms_dragObjReqSlots.clear();

			SlottedContainmentProperty* slottedDragObj = ContainerInterface::getSlottedContainmentProperty(*clientDragObj);
			if(slottedDragObj)
			{
				std::vector<int> validArrangements;
				Container::ContainerErrorCode ec;
				slotContainer->getValidArrangements(*clientDragObj, validArrangements, ec, true, false);
				for(std::vector<int>::size_type i = 0; i < validArrangements.size(); ++i)
				{
					SlottedContainmentProperty::SlotArrangement const slotsDO = slottedDragObj->getSlotArrangement(validArrangements[i]); 
					for(unsigned int j = 0; j < slotsDO.size(); ++j)
					{
						ms_dragObjReqSlots.push_back(CrcLowerString(SlotIdManager::getSlotName(slotsDO[j]).getString()));
					}
				}

				ms_currentDragObject = clientDragObj;
			}
		}
	}

	
	for(unsigned int i = 0; i < m_slotViewers.size(); ++i)
	{
		bool valid = i < ms_totalSlotNames && slotContainer->hasSlot(SlotIdManager::findSlotId(ms_slotStringCrcs[i]));

		m_slotViewers[i]->SetVisible(valid);
		m_slotViewers[i]->SetEnabled(valid);

		m_slotText[i]->SetVisible(valid);
		m_slotText[i]->SetEnabled(valid);

		if(valid)
		{
			m_slotViewers[i]->clearObjects();
			m_slotViewers[i]->SetTooltip(UIString());
			ClientObject * appObject = ContainerInterface::getObjectInSlot(*itemContainer, ms_slotNames[i]);
			if(!strcmp(ms_slotNames[i], ms_defaultPants) && !appObject)
					appObject = ContainerInterface::getObjectInSlot(*itemContainer, ms_alternatePants);
			
			m_slotText[i]->SetName(Unicode::wideToNarrow(ms_slotDisplayNames[i].localize()));
			m_slotText[i]->SetLocalText(ms_slotDisplayNames[i].localize());

			if(appObject)
			{
				m_slotViewers[i]->setObject(appObject);
				m_slotViewers[i]->SetTooltip(appObject->getLocalizedName());
			}

			if(!ms_dragObjReqSlots.empty() && std::find(ms_dragObjReqSlots.begin(), ms_dragObjReqSlots.end(), ms_slotStringCrcs[i]) != ms_dragObjReqSlots.end())
			{
				if(!m_showInventoryItems->IsChecked() && ms_currentDragObject && ContainerInterface::getContainer(*ms_currentDragObject) && ContainerInterface::getContainer(*ms_currentDragObject)->getNumberOfItems() != 0)
					m_slotViewers[i]->SetBackgroundTint(UIColor::red); // No Containers with items while using Appearance Objects.
				else if(appObject && !m_showInventoryItems->IsChecked())
					m_slotViewers[i]->SetBackgroundTint(UIColor::red); // An object is already in that slot.
				else if(appObject && m_showInventoryItems->IsChecked())
					m_slotViewers[i]->SetBackgroundTint(UIColor::yellow); // Object is in that slot, but it will be automatically removed.
				else
					m_slotViewers[i]->SetBackgroundTint(UIColor::green); // Slot is empty and ready.
			}
			else
				m_slotViewers[i]->SetBackgroundTint(defaultBGColor);
		}
	}

	if(playerCreature->getDupedCreaturesDirty())
	{
		CleanupDupedCreature();
		
		ms_dupedCreatureNoAppearance = SwgCuiAvatarCreationHelper::duplicateCreatureWithClothesAndCustomization(*playerCreature, false);

		if(m_showInventoryItems->IsChecked() && ms_dupedCreatureNoAppearance)
		{
			Appearance * const app = ms_dupedCreatureNoAppearance->getAppearance();
			if(app)
			{
				SkeletalAppearance2 * const skelApp = app->asSkeletalAppearance2();
				if(skelApp)
				{
					skelApp->setDetailLevel(0);
				}
			}

			m_characterViewer->setObject(ms_dupedCreatureNoAppearance);
			m_characterViewer->setAlterObjects(true);
			m_characterViewer->setCameraForceTarget   (false);
			m_characterViewer->setCameraAutoZoom      (true);
			m_characterViewer->setCameraLookAtBone    ("root");
			m_characterViewer->setCameraZoomLookAtBone("head");
			m_characterViewer->setViewDirty(true);
			m_characterViewer->recomputeZoom();
		}

		playerCreature->setDupedCreaturesDirty(false);
	}

	if(m_dragTimer.updateNoReset(delta))
	{
		m_lastDragItem->clear();
		m_dragTimer.reset();
		ms_dragObjReqSlots.clear();
	}
}

void SwgCuiAppearanceTab::performActivate()
{
	if (!m_embeddedInCharacterSheet)
		CuiManager::requestPointer(true);
	layoutForEmbeddedParent();
	m_characterViewer->setPaused(false);
	setIsUpdating(true);

	CreatureObject * const playerCreature = Game::getPlayerCreature();
	if (!playerCreature)
		return;

	if(m_characterViewer->getLastObject() != playerCreature && m_characterViewer->getLastObject() != ms_dupedCreatureNoAppearance)
	{
		m_characterViewer->setObject(playerCreature);
		if(ms_dupedCreatureNoAppearance)
			CleanupDupedCreature();
	}

	if(!ms_dupedCreatureNoAppearance)
		ms_dupedCreatureNoAppearance = SwgCuiAvatarCreationHelper::duplicateCreatureWithClothesAndCustomization(*playerCreature, false);
}

void SwgCuiAppearanceTab::performDeactivate()
{
	if (!m_embeddedInCharacterSheet)
		CuiManager::requestPointer(false);
	m_characterViewer->setPaused(true);
	setIsUpdating(false);
}

void SwgCuiAppearanceTab::OnButtonPressed(UIWidget *context)
{
	if(context == m_closeButton && !m_embeddedInCharacterSheet)
		closeNextFrame();
}

void SwgCuiAppearanceTab::OnCheckboxSet(UIWidget * context)
{
	if(context == m_showInventoryItems && ms_dupedCreatureNoAppearance)
	{
		Appearance * const app = ms_dupedCreatureNoAppearance->getAppearance();
		if(app)
		{
			SkeletalAppearance2 * const skelApp = app->asSkeletalAppearance2();
			if(skelApp)
			{
				skelApp->setDetailLevel(0);
			}
		}

		m_characterViewer->setObject(ms_dupedCreatureNoAppearance);
		m_characterViewer->setAlterObjects(true);
		m_characterViewer->setCameraForceTarget   (false);
		m_characterViewer->setCameraAutoZoom      (true);
		m_characterViewer->setCameraLookAtBone    ("root");
		m_characterViewer->setCameraZoomLookAtBone("head");
		m_characterViewer->setViewDirty(true);
		m_characterViewer->recomputeZoom();
	}
}

void SwgCuiAppearanceTab::OnCheckboxUnset(UIWidget * context)
{
	if(context == m_showInventoryItems && Game::getPlayerCreature())
	{
		m_characterViewer->setObject(Game::getPlayerCreature());
		m_characterViewer->setAlterObjects(false);
		m_characterViewer->setViewDirty(true);
		m_characterViewer->recomputeZoom();
	}
}

bool SwgCuiAppearanceTab::OnMessage(UIWidget *context, const UIMessage & msg )
{
	if (msg.Type == UIMessage::DragOver)
	{
		context->SetDropFlagOk (true);
		m_lastDragItem->set (*msg.DragObject);
		m_dragTimer.reset();

		return false;
	}
	else if (msg.Type == UIMessage::DragCancel)
	{
		m_lastDragItem->clear();

		return false;
	}
	else if (msg.Type == UIMessage::DragEnd)
	{
		m_lastDragItem->clear();
		m_dragTimer.reset();
		ms_dragObjReqSlots.clear();

		if (msg.DragSource == context)
			return false;

		CuiDragInfo droppingInfo (*msg.DragObject);

		ClientObject * dragObj = droppingInfo.getClientObject();

		if(!dragObj)
			return false;

		TangibleObject * tangible = dragObj->asTangibleObject();

		if(!tangible)
			return false;

		ClientObject* dragObjContainer = dynamic_cast<ClientObject*>(ContainerInterface::getContainedByObject(*dragObj));

		if(!Game::getPlayerCreature() || !Game::getPlayerCreature()->getInventoryObject())
			return false;
		
		if(dragObjContainer == Game::getPlayerCreature()->getInventoryObject() && !m_showInventoryItems->IsChecked())
		{
			SharedObjectTemplate::GameObjectType got  = SharedObjectTemplate::GOT_none;
			if (dragObj)
				got = static_cast<SharedObjectTemplate::GameObjectType> (dragObj->getGameObjectType ());

			if( ( GameObjectTypes::isTypeOf (got, SharedObjectTemplate::GOT_armor) ||	
				GameObjectTypes::isTypeOf (got, SharedObjectTemplate::GOT_clothing)||
				GameObjectTypes::isTypeOf (got, SharedObjectTemplate::GOT_jewelry) ||
				got == SharedObjectTemplate::GOT_misc_appearance_only ||
				got == SharedObjectTemplate::GOT_misc_appearance_only_invisible ||
				got == SharedObjectTemplate::GOT_misc_container_wearable ) && 
				ClientObjectUsabilityManager::canWear(Game::getPlayerCreature(), tangible))
			{
				bool isSkeletalAppearance2 = tangible->getAppearance() ? tangible->getAppearance()->asSkeletalAppearance2() : false;
				if(isSkeletalAppearance2)
				{
					CuiInventoryManager::equipAppearanceItem(dragObj->getNetworkId());
					return true;
				}

			}
			
			return false;
		}
		else if (dragObjContainer == Game::getPlayerCreature()->getInventoryObject() && m_showInventoryItems->IsChecked())
		{
			if(ClientObjectUsabilityManager::canWear(Game::getPlayerCreature(), tangible))
			{
				TangibleObject * playerTangible =Game::getPlayer()->asClientObject()->asTangibleObject();
				if(playerTangible && tangible->getGameObjectType() != SharedObjectTemplate::GOT_misc_appearance_only && tangible->getGameObjectType() != SharedObjectTemplate::GOT_misc_appearance_only_invisible)
					CuiInventoryManager::equipItem(*tangible, *playerTangible);
			}
		}

	}
	else if(msg.Type == UIMessage::LeftMouseDoubleClick)
	{
		if(!CuiPreferences::getDoubleClickUnequipAppearance())
			return true;

		if(context == m_viewerPage)
			return false;
	
		UIWidget * widgetUnderMouse = getPage().GetWidgetFromPoint( msg.MouseCoords, true );
		CuiWidget3dObjectListViewer *viewer = dynamic_cast<CuiWidget3dObjectListViewer *>(widgetUnderMouse);

		if(viewer)
		{
			Object * item = viewer->getLastObject();
			TangibleObject * tangItem = item ? item->asClientObject()->asTangibleObject() : NULL;

			TangibleObject * playerTangible = Game::getPlayerCreature() ? Game::getPlayerCreature()->asTangibleObject() : NULL;

			if(tangItem && playerTangible)
			{
				if(GameObjectTypes::isTypeOf (tangItem->getGameObjectType(), SharedObjectTemplate::GOT_cybernetic))
				{
					// System message
					CuiSystemMessageManager::sendFakeSystemMessage (ms_cybernetics_error.localize());
					return true;
				}

				if(!m_showInventoryItems->IsChecked())
					CuiInventoryManager::unequipAppearanceItem(item->getNetworkId());
				else
				{
					CuiInventoryManager::unequipItem(*tangItem, *playerTangible);
				}
			}

			return true;
		}	
	}

	return true;
}
