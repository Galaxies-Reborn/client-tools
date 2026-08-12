//======================================================================
//
// SwgCuiOptGalaxiesReborn.cpp
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiOptGalaxiesReborn.h"

#include "UICheckbox.h"
#include "clientUserInterface/CuiPreferences.h"

//======================================================================

SwgCuiOptGalaxiesReborn::SwgCuiOptGalaxiesReborn(UIPage & page) :
SwgCuiOptBase("SwgCuiOptGalaxiesReborn", page)
{
	UICheckbox * checkbox = 0;
	getCodeDataObject(TUICheckbox, checkbox, "checkHamEnhance", true);
	if (checkbox)
	{
		registerCheckbox(
			*checkbox,
			CuiPreferences::setHamEnhance,
			CuiPreferences::getHamEnhance,
			SwgCuiOptBase::getFalse);
	}
}

//======================================================================
