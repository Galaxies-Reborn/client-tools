//======================================================================
//
// SwgCuiOptGalaxiesReborn.h
//
//======================================================================

#ifndef INCLUDED_SwgCuiOptGalaxiesReborn_H
#define INCLUDED_SwgCuiOptGalaxiesReborn_H

#include "swgClientUserInterface/SwgCuiOptBase.h"

//======================================================================

class UISliderbar;

class SwgCuiOptGalaxiesReborn : public SwgCuiOptBase
{
public:
	explicit SwgCuiOptGalaxiesReborn(UIPage & page);
	~SwgCuiOptGalaxiesReborn() {}
	virtual void resetDefaults(bool confirmed);

private:
	static void onChatUiScaleChanged(SwgCuiOptBase const & base, UISliderbar const & slider, float value);

	SwgCuiOptGalaxiesReborn();
	SwgCuiOptGalaxiesReborn(SwgCuiOptGalaxiesReborn const &);
	SwgCuiOptGalaxiesReborn & operator=(SwgCuiOptGalaxiesReborn const &);
};

//======================================================================

#endif
