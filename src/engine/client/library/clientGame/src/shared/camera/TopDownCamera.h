//===================================================================
//
// TopDownCamera.h
// copyright (c) 2026 Galaxies Reborn
//
// An overhead camera the player pans, zooms and orbits, for point-and-click control.
//
// Modelled on StructurePlacementCamera, which already pans by edge-scroll, follows terrain and sits
// at a steep pitch -- but that one is welded to structure placement: it owns a footprint, a create
// location and lot-validity shaders, and it draws the placement overlay. This is the same rig
// without any of that, plus the things a general camera needs and a placement camera does not:
// clamped pitch, orbit, and the ability to detach from the avatar and pan freely.
//
//===================================================================

#ifndef INCLUDED_TopDownCamera_H
#define INCLUDED_TopDownCamera_H

//===================================================================

#include "clientObject/GameCamera.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Watcher.h"

class MessageQueue;
class Object;

//===================================================================

class TopDownCamera : public GameCamera
{
public:

	TopDownCamera ();
	virtual ~TopDownCamera ();

	virtual void  setActive (bool active);
	virtual float alter (float time);

	void          setMessageQueue (const MessageQueue* queue);
	void          setTarget (const Object* target);
	void          setMouseCoordinates (int x, int y);

	//-- Panning detaches the camera from the avatar; this puts it back. A player who has panned
	//   across town to watch a workshop should not be dragged back by their character walking.
	void          recenterOnTarget ();

	void          zoom (float delta);
	void          orbit (float yawDelta, float pitchDelta);

	//-- Whether the pivot follows the avatar. False once the player pans away, true again on
	//   recentre.
	bool          isFollowing () const;
	void          setFollowing (bool following);

	//-- Where the pivot currently sits, which is what a click-to-move destination is measured
	//   against and what the interaction code needs to know.
	const Vector& getPivot () const;

private:

	enum Keys
	{
		K_up,
		K_down,
		K_left,
		K_right,

		K_COUNT
	};

private:

	const MessageQueue*  m_queue;
	bool                 m_keys [K_COUNT];
	ConstWatcher<Object> m_target;
	Vector               m_pivot;
	float                m_zoom;
	float                m_yaw;
	float                m_pitch;
	int                  m_mouseX;
	int                  m_mouseY;
	bool                 m_following;
	bool                 m_pivotValid;

private:

	TopDownCamera (const TopDownCamera&);
	TopDownCamera& operator= (const TopDownCamera&);
};

//===================================================================

#endif
