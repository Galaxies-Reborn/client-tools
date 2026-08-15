//===================================================================
//
// RemoteShipController.h
// Copyright 2003 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
//===================================================================

#ifndef INCLUDED_RemoteShipController_H
#define INCLUDED_RemoteShipController_H

//===================================================================

#include "clientGame/ShipController.h"

//===================================================================

class MessageQueueDataTransform;
class MessageQueueDataTransformWithParent;
class ShipObject;

//===================================================================

class RemoteShipController : public ShipController
{
public:

	static void install ();

public:

	explicit RemoteShipController(ShipObject * owner);
	virtual ~RemoteShipController();

	virtual void endBaselines();
	virtual void receiveTransform(ShipUpdateTransformMessage const & shipUpdateTransformMessage);

	Transform const & getServerTransform_p() const;

	// hyperspace methods
	void enterByHyperspace();
	void leaveByHyperspace();
	void enterByAtmosphere();
	void leaveByAtmosphere();

protected:

	virtual float realAlter(float elapsedTime);
	virtual void handleNetUpdateTransform(MessageQueueDataTransform const & message);
	virtual void handleNetUpdateTransformWithParent(MessageQueueDataTransformWithParent const & message);

private:

	static void remove ();

private:

	RemoteShipController();
	RemoteShipController(RemoteShipController const &);
	RemoteShipController & operator=(RemoteShipController const &);

private:

	uint32 m_serverToClientLastSyncStamp;
	Transform m_serverTransform_p;

	// hyperspace members
	bool m_isInHyperspace;
	bool m_killShipWhenFinishedHyperspace;
	float m_totalElapsedTimeInHyperspaceSeconds;
	Transform m_transformAfterHyperspace;

	// ground atmospheric arrival members
	bool m_isInAtmosphericArrival;
	float m_totalElapsedTimeInAtmosphericArrivalSeconds;
	Transform m_transformAfterAtmosphericArrival;

	// ground atmospheric departure members
	bool m_isInAtmosphericDeparture;
	float m_totalElapsedTimeInAtmosphericDepartureSeconds;
	Transform m_transformBeforeAtmosphericDeparture;
};

//----------------------------------------------------------------------

inline Transform const & RemoteShipController::getServerTransform_p() const
{
	return m_serverTransform_p;
}

//===================================================================

#endif
