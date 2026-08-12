//======================================================================
//
// SwgCuiOptGalaxiesReborn.h
//
//======================================================================

#ifndef INCLUDED_SwgCuiOptGalaxiesReborn_H
#define INCLUDED_SwgCuiOptGalaxiesReborn_H

#include "swgClientUserInterface/SwgCuiOptBase.h"

//======================================================================

class SwgCuiOptGalaxiesReborn : public SwgCuiOptBase
{
public:
	explicit SwgCuiOptGalaxiesReborn(UIPage & page);
	~SwgCuiOptGalaxiesReborn() {}

private:
	SwgCuiOptGalaxiesReborn();
	SwgCuiOptGalaxiesReborn(SwgCuiOptGalaxiesReborn const &);
	SwgCuiOptGalaxiesReborn & operator=(SwgCuiOptGalaxiesReborn const &);
};

//======================================================================

#endif
