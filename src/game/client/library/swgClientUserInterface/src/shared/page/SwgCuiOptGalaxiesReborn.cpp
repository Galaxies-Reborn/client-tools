//======================================================================
//
// SwgCuiOptGalaxiesReborn.cpp
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiOptGalaxiesReborn.h"
#include "swgClientUserInterface/SwgCuiChatWindow.h"

#include "UICheckbox.h"
#include "UISliderbar.h"
#include "clientUserInterface/CuiPreferences.h"

//======================================================================

SwgCuiOptGalaxiesReborn::SwgCuiOptGalaxiesReborn(UIPage & page) :
SwgCuiOptBase("SwgCuiOptGalaxiesReborn", page)
{
	UICheckbox * checkbox = 0;
	getCodeDataObject(TUICheckbox, checkbox, "checkHamEnhance", true);
	if (checkbox)
	{
		checkbox->SetChecked(true, false);
		checkbox->SetEnabled(false);
		registerCheckbox(
			*checkbox,
			CuiPreferences::setHamEnhance,
			CuiPreferences::getHamEnhance,
			SwgCuiOptBase::getFalse);
	}

	UISliderbar * slider = 0;
	getCodeDataObject(TUISliderbar, slider, "sliderSkillBarUiScale", true);
	if (slider)
	{
		registerSlider(*slider, CuiPreferences::setSkillBarUiScale, CuiPreferences::getSkillBarUiScale,
			SwgCuiOptBase::getOne, CuiPreferences::getIndividualUiScaleMinimum(), CuiPreferences::getIndividualUiScaleMaximum());
	}

	slider = 0;
	getCodeDataObject(TUISliderbar, slider, "sliderHamUiScale", true);
	if (slider)
	{
		registerSlider(*slider, CuiPreferences::setHamUiScale, CuiPreferences::getHamUiScale,
			SwgCuiOptBase::getOne, CuiPreferences::getIndividualUiScaleMinimum(), CuiPreferences::getIndividualUiScaleMaximum());
	}

	slider = 0;
	getCodeDataObject(TUISliderbar, slider, "sliderPartyUiScale", true);
	if (slider)
	{
		registerSlider(*slider, CuiPreferences::setPartyUiScale, CuiPreferences::getPartyUiScale,
			SwgCuiOptBase::getOne, CuiPreferences::getIndividualUiScaleMinimum(), CuiPreferences::getIndividualUiScaleMaximum());
	}

	slider = 0;
	getCodeDataObject(TUISliderbar, slider, "sliderChatUiScale", true);
	if (slider)
	{
		registerSlider(*slider, CuiPreferences::setChatUiScale, CuiPreferences::getChatUiScale,
			SwgCuiOptBase::getOne, CuiPreferences::getIndividualUiScaleMinimum(), CuiPreferences::getIndividualUiScaleMaximum(),
			onChatUiScaleChanged);
	}

	slider = 0;
	getCodeDataObject(TUISliderbar, slider, "sliderMenuUiScale", true);
	if (slider)
	{
		registerSlider(*slider, CuiPreferences::setMenuUiScale, CuiPreferences::getMenuUiScale,
			SwgCuiOptBase::getOne, CuiPreferences::getIndividualUiScaleMinimum(), CuiPreferences::getIndividualUiScaleMaximum());
	}
}

//----------------------------------------------------------------------

void SwgCuiOptGalaxiesReborn::onChatUiScaleChanged(SwgCuiOptBase const &, UISliderbar const &, float const value)
{
	SwgCuiChatWindow::applyUiScaleToAllWindows(value);
}

//----------------------------------------------------------------------

void SwgCuiOptGalaxiesReborn::resetDefaults(bool const confirmed)
{
	SwgCuiOptBase::resetDefaults(confirmed);
	if (confirmed)
		SwgCuiChatWindow::applyUiScaleToAllWindows(CuiPreferences::getChatUiScale());
}

//======================================================================
