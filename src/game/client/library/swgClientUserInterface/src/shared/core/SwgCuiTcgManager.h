// ======================================================================
//
// SwgCuiTcgManager.h
// copyright (c) 2008 Sony Online Entertainment LLC
//
// ======================================================================

#ifndef INCLUDED_SwgCuiTcgManager_H
#define INCLUDED_SwgCuiTcgManager_H

// ======================================================================

class SwgCuiTcgManager
{
public:
	enum LaunchResult
	{
		LR_failed,
		LR_embedded
	};

	static void install();
	static void remove();
	static void update(float deltaTimeSecs);
	static LaunchResult performAction(char const * integrationActionNonce = 0);
	static bool isIntegrationGameActionDispatch();
	static char const * getIntegrationGameActionNonce();
	static LaunchResult launch();
	static void setLoginInfo(char const * const username, char const * const sessionId);

private: //disabled
	SwgCuiTcgManager();
	SwgCuiTcgManager(const SwgCuiTcgManager &rhs);
	SwgCuiTcgManager& operator= (const SwgCuiTcgManager &rhs);

private:
};

// ======================================================================

#endif
