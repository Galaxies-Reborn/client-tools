//======================================================================
//
// ClientBuffManager.h
// copyright (c) 2003 Sony Online Entertainment
//
//======================================================================

#ifndef INCLUDED_ClientBuffManager_H
#define INCLUDED_ClientBuffManager_H

//======================================================================

class UIImage;
class UIImageStyle;
class Buff;

//----------------------------------------------------------------------

class ClientBuffManager
{
public:
	struct CatalogAudit
	{
		CatalogAudit();

		uint32 recordCount;
		uint32 visibleCount;
		uint32 positiveCount;
		uint32 debuffCount;
		uint32 authoredIconMissCount;
		uint32 unresolvedIconCount;
	};

	struct StatusPanelAudit
	{
		StatusPanelAudit();

		bool diagnosticsValid;
		bool panelVisible;
		bool hasPositiveFixture;
		bool hasDebuffFixture;
		uint32 visibleCount;
		uint32 positiveCount;
		uint32 debuffCount;
		uint32 renderedIconCount;
	};

	static void install();
	static void remove();

	static int  getBuffState(uint32 buffNameCrc);
	static float getBuffDefaultDuration(uint32 buffNameCrc);
	static bool getBuffIsDebuff(uint32 buffNameCrc);
	static bool getBuffIsGroupVisible(uint32 buffNameCrc);
	static bool getBuffGroupAndPriority(uint32 buffNameCrc, uint32 & group1Crc, uint32 & group2Crc, int & priority);
	static int  getBuffMaxStacks(uint32 buffNameCrc);
	static bool getBuffIsCelestial(uint32 buffNameCrc);
	static bool getBuffIsDispellable(uint32 buffNameCrc);
	static int  getBuffDisplayOrder(uint32 buffNameCrc);

	static UIImageStyle * getBuffIconStyle(uint32 buffNameCrc);
	static void auditVisibleBuffCatalog(CatalogAudit & result);
	static bool applyStatusPanelDebugFixtures(int durationSeconds, bool refresh);
	static bool clearStatusPanelDebugFixtures();
	static bool getStatusPanelAudit(StatusPanelAudit & result);
	static void setStatusPanelDiagnostics(uint32 renderedIconCount, bool panelVisible);
	static void getBuffDescription(Buff const & buff, Unicode::String & result);
	static void addTimestampToBuffDescription(Unicode::String const & description, int timeLeft, Unicode::String & result);

protected:
	
};


//======================================================================

#endif
