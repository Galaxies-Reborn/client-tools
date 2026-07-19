// ======================================================================
//
// CuiDataDrivenPageManager.h
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_CuiDataDrivenPageManager_H
#define INCLUDED_CuiDataDrivenPageManager_H

// ======================================================================

#include "sharedNetworkMessages/ServerUserInterfaceMessages.h"

class CuiDataDrivenPage;
class SuiPageData;
class SuiCreatePageMessage;
class SuiUpdatePageMessage;

//-----------------------------------------------------------------

/**
 *  This class manages server-side UI creation.  This allows scripters to specify some simple
 *  "fill-in-the-blanks" pages without much time being spent by a UI programmer.
 */
class CuiDataDrivenPageManager
{
public:

	typedef std::vector<SuiCreatePage::Command> CommandVector;

	static void install           ();
	static void remove            ();
	static void createPage        (SuiPageData const &);
	static void closePage         (int pageId);
	static void handleSceneChange ();

	static void receiveCreatePageMessage      (SuiCreatePageMessage const &);
	static void receiveForceCloseMessage      (SuiForceClosePage const &);
	static void receiveUpdatePageMessage      (SuiUpdatePageMessage const &);
	
	static void removePage        (CuiDataDrivenPage* page, bool alreadyClosing);

	// Background acceptance seam for the identity-bound clone lifecycle.
	// This succeeds only when exactly one active Script.listBox page has the
	// requested row and an enabled OK button. The normal list and button
	// callbacks still emit the authoritative SUI notifications. Selection
	// and confirmation are separate so the server-authored prompt update can
	// round-trip before the close event.
	// Returns 1 after the requested real selection or server-acknowledged OK
	// callback. Failure values retain a diagnostic observation mask for the
	// opt-in acceptance bridge.
	static int selectOrConfirmSingleListRow (int row, bool confirm);

private:
	//disabled
	CuiDataDrivenPageManager            (const CuiDataDrivenPageManager &rhs);
	CuiDataDrivenPageManager& operator= (const CuiDataDrivenPageManager &rhs);

private:

private:
	typedef stdmap<int, CuiDataDrivenPage*>::fwd PageMap;

	static bool                                  ms_installed;
	static PageMap                               ms_pages;
};

// ======================================================================

#endif
