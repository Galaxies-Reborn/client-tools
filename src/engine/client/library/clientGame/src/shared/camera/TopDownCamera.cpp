//===================================================================
//
// TopDownCamera.cpp
// copyright (c) 2026 Galaxies Reborn
//
//===================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/TopDownCamera.h"

#include "clientGraphics/Graphics.h"
#include "clientTerrain/ClientProceduralTerrainAppearance.h"
#include "sharedFoundation/GameControllerMessage.h"
#include "sharedFoundation/MessageQueue.h"
#include "sharedObject/AlterResult.h"
#include "sharedTerrain/TerrainObject.h"

//===================================================================

namespace TopDownCameraNamespace
{
	//-- How far the camera sits from its pivot, and the range the wheel may drive it through.
	//
	//   The near end is close enough to read a shopfront and a character's gear; the far end shows a
	//   street plan. Beyond about 90 the terrain's own detail reduction starts to show and the scene
	//   stops being worth looking at.
	const float cms_zoomNear    = 8.f;
	const float cms_zoomFar     = 90.f;
	const float cms_zoomDefault = 28.f;
	const float cms_zoomRate    = 0.08f;

	//-- Pitch is clamped rather than free. Straight down reads badly on SWG's terrain and hides the
	//   building facades that carry the art; too shallow and the camera stops being overhead at all
	//   and the player loses the plan view that makes click-to-move legible.
	//
	//   The default sits near 55 degrees: high enough to read a street, low enough to see a shopfront.
	const float cms_pitchMin     = PI / 5.14f;   // ~35 degrees
	const float cms_pitchMax     = PI / 2.25f;   // ~80 degrees
	const float cms_pitchDefault = PI / 3.27f;   // ~55 degrees

	//-- How fast edge-scroll and the pan keys move the pivot, in world units per second. Scaled by
	//   zoom so a zoomed-out camera pans proportionally faster; panning a rooftop view at ground
	//   speed feels broken.
	const float cms_panRate = 0.9f;

	//-- How far the pivot may stray from the avatar before it stops. Without a limit a player can
	//   pan into unloaded terrain and lose their character entirely.
	const float cms_maxPivotDistance = 260.f;

	//-- Edge-scroll margin in pixels.
	const int cms_edgeMargin = 3;
}

using namespace TopDownCameraNamespace;

//===================================================================

TopDownCamera::TopDownCamera () :
	GameCamera (),
	m_queue (0),
	m_target (),
	m_pivot (),
	m_zoom (cms_zoomDefault),
	m_yaw (0.f),
	m_pitch (cms_pitchDefault),
	m_mouseX (0),
	m_mouseY (0),
	m_following (true),
	m_pivotValid (false)
{
	int i;
	for (i = 0; i < K_COUNT; ++i)
		m_keys [i] = false;
}

//-------------------------------------------------------------------

TopDownCamera::~TopDownCamera ()
{
	m_queue = 0;
}

//-------------------------------------------------------------------

void TopDownCamera::setActive (bool active)
{
	GameCamera::setActive (active);

	//-- Snap to the avatar when the view is entered. Coming back to a camera still pointed wherever
	//   it was left is disorienting, and there is no way for the player to tell why.
	if (active)
		recenterOnTarget ();
}

//-------------------------------------------------------------------

void TopDownCamera::setMessageQueue (const MessageQueue* newQueue)
{
	m_queue = newQueue;
}

//-------------------------------------------------------------------

void TopDownCamera::setTarget (const Object* target)
{
	m_target = target;

	if (m_target && !m_pivotValid)
		recenterOnTarget ();
}

//-------------------------------------------------------------------

void TopDownCamera::setMouseCoordinates (int x, int y)
{
	m_mouseX = x;
	m_mouseY = y;
}

//-------------------------------------------------------------------

void TopDownCamera::recenterOnTarget ()
{
	if (!m_target)
		return;

	m_pivot = m_target->getPosition_w ();
	m_pivotValid = true;
	m_following = true;
}

//-------------------------------------------------------------------

void TopDownCamera::zoom (float delta)
{
	//-- Proportional to the current distance, so one wheel click covers a similar fraction of the
	//   view at every zoom level rather than being enormous when close and imperceptible when far.
	m_zoom += delta * m_zoom * cms_zoomRate;

	if (m_zoom < cms_zoomNear)
		m_zoom = cms_zoomNear;

	if (m_zoom > cms_zoomFar)
		m_zoom = cms_zoomFar;
}

//-------------------------------------------------------------------

void TopDownCamera::orbit (float yawDelta, float pitchDelta)
{
	m_yaw += yawDelta;

	while (m_yaw > PI_TIMES_2)
		m_yaw -= PI_TIMES_2;

	while (m_yaw < 0.f)
		m_yaw += PI_TIMES_2;

	m_pitch += pitchDelta;

	if (m_pitch < cms_pitchMin)
		m_pitch = cms_pitchMin;

	if (m_pitch > cms_pitchMax)
		m_pitch = cms_pitchMax;
}

//-------------------------------------------------------------------

bool TopDownCamera::isFollowing () const
{
	return m_following;
}

//-------------------------------------------------------------------

void TopDownCamera::setFollowing (bool following)
{
	m_following = following;

	if (m_following)
		recenterOnTarget ();
}

//-------------------------------------------------------------------

const Vector& TopDownCamera::getPivot () const
{
	return m_pivot;
}

//-------------------------------------------------------------------

float TopDownCamera::alter (float elapsedTime)
{
	if (!isActive ())
		return AlterResult::cms_alterNextFrame;

	if (!m_target)
		return AlterResult::cms_alterNextFrame;

	if (!m_pivotValid)
		recenterOnTarget ();

	int i;
	for (i = 0; i < K_COUNT; ++i)
		m_keys [i] = false;

	//-- The pan keys are the same movement messages the avatar uses. While this camera is the active
	//   view they steer the camera instead, which is what an overhead game does: the avatar is
	//   directed by clicking, not driven.
	if (m_queue)
	{
		for (i = 0; i < m_queue->getNumberOfMessages (); ++i)
		{
			int   message;
			float value;

			m_queue->getMessage (i, &message, &value);

			switch (message)
			{
			case CM_walk:  m_keys [K_up]    = true; break;
			case CM_down:  m_keys [K_down]  = true; break;
			case CM_left:  m_keys [K_left]  = true; break;
			case CM_right: m_keys [K_right] = true; break;
			default:
				break;
			}
		}
	}

	//-- Pan, in the camera's own yaw frame so that "up" is up the screen regardless of orbit.
	float const panDistance = cms_panRate * m_zoom * elapsedTime;

	float const sinYaw = sin (m_yaw);
	float const cosYaw = cos (m_yaw);

	Vector const forward (sinYaw, 0.f, cosYaw);
	Vector const rightward (cosYaw, 0.f, -sinYaw);

	Vector pan;

	if (m_keys [K_up] || m_mouseY < cms_edgeMargin)
		pan += forward;

	if (m_keys [K_down] || m_mouseY > Graphics::getCurrentRenderTargetHeight () - cms_edgeMargin)
		pan -= forward;

	if (m_keys [K_right] || m_mouseX > Graphics::getCurrentRenderTargetWidth () - cms_edgeMargin)
		pan += rightward;

	if (m_keys [K_left] || m_mouseX < cms_edgeMargin)
		pan -= rightward;

	if (pan.normalize ())
	{
		m_pivot += pan * panDistance;

		//-- Any deliberate pan detaches from the avatar. Following resumes only on an explicit
		//   recentre, so a character walking home does not drag the view off whatever the player
		//   was watching.
		m_following = false;
	}

	if (m_following)
		m_pivot = m_target->getPosition_w ();

	//-- Keep the pivot within reach of the avatar. Panning into unloaded terrain loses the character
	//   with no obvious way back.
	{
		Vector const targetPosition = m_target->getPosition_w ();

		Vector offset = m_pivot - targetPosition;
		offset.y = 0.f;

		float const distance = offset.magnitude ();

		if (distance > cms_maxPivotDistance && offset.normalize ())
			m_pivot = targetPosition + offset * cms_maxPivotDistance;
	}

	//-- Sit the pivot on the ground, so zoom and pitch are measured from the terrain rather than
	//   from whatever height the pivot drifted to.
	const TerrainObject* const terrain = TerrainObject::getConstInstance ();

	if (terrain)
		IGNORE_RETURN (terrain->getHeight (m_pivot, m_pivot.y));

	Transform t;
	t.setPosition_p (m_pivot);
	t.yaw_l (m_yaw);
	t.pitch_l (m_pitch);
	t.move_l (Vector (0.f, 0.f, -m_zoom));

	setTransform_o2p (t);

	return GameCamera::alter (elapsedTime);
}

//===================================================================
