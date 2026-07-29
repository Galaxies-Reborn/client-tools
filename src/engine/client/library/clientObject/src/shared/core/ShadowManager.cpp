// ======================================================================
// 
// ShadowManager.cpp
// asommers
//
// copyright 2003, sony online entertainment
//
// ======================================================================

#include "clientObject/FirstClientObject.h"
#include "clientObject/ShadowManager.h"

#include "clientGraphics/Camera.h"
#include "clientGraphics/Graphics.h"
#include "clientObject/ShadowBlobManager.h"
#include "clientObject/ShadowVolume.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedUtility/LocalMachineOptionManager.h"

// ======================================================================

namespace ShadowManagerNamespace
{
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	enum SkeletalShadowType
	{
		SST_none,
		SST_simple,
		SST_volumetric
	};

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	const float cms_minimumVolumetricShadowDistance = 128.f;
	const float cms_maximumVolumetricShadowDistance = 2048.f;
	const float cms_minimumSimpleShadowDistance     = 32.f;
	const float cms_maximumSimpleShadowDistance     = 1024.f;

	// The "too small to bother with" cull, as a fraction of render target width. At the shipped
	// 10/800 that is a 24 pixel radius at 1920, and a 1m radius character at 100m projects to about
	// 17 -- so shadows were being dropped by apparent size long before any distance limit applied,
	// and nothing exposed it. Now it moves with the shadow detail slider, whose bottom end is the
	// value that shipped.
	const float cms_maximumIgnoreRatio              = 10.f / 800.f;
	const float cms_minimumIgnoreRatio              =  1.f / 800.f;

	const float cms_fogNaturalBase					= 2.71828f;

	// Fog also cuts shadows off, at 80% obscured. That is a reasonable place to stop when the draw
	// distance is short and a visible one when it is not, so it moves with the same slider.
	const float cms_maximumFogCutOff				= 0.20f;
	const float cms_minimumFogCutOff				= 0.02f;

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	bool  ms_enabled                  = false;
	bool ms_allowed = true;
	bool  ms_debugReport              = false;
	float ms_maximumVolumetricShadowDistance = cms_minimumVolumetricShadowDistance;
	float ms_volumetricShadowDistanceLevel = 0.f;
	float ms_shadowDetailLevel        = 0.f;
	float ms_volumetricShadowDistance = cms_minimumVolumetricShadowDistance;
	bool  ms_meshShadows              = false;
	float ms_simpleShadowDistance     = cms_minimumSimpleShadowDistance;
	int   ms_skeletalShadows          = SST_none;
	float ms_timeOfDay				  = 0.0f;
	
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	void remove ()
	{
		DebugFlags::unregisterFlag (ms_debugReport);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	void updateShadows ()
	{
		ShadowBlobManager::setEnabled (ms_skeletalShadows == SST_simple);
		ShadowVolume::setEnabled (ms_meshShadows || ms_skeletalShadows == SST_volumetric);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	inline bool shouldRender (Camera const & camera, Vector const & position_w, float const radius, float const maximumDistance)
	{
		//-- check maximum distance first: a subtraction and a compare against a squared distance,
		//   and it rejects far more than the size test does.
		const float distanceSquared = camera.getPosition_w ().magnitudeBetweenSquared (position_w);
		if (distanceSquared > sqr (maximumDistance))
			return false;

		const float ignoreRatio = linearInterpolate (cms_maximumIgnoreRatio, cms_minimumIgnoreRatio, ms_shadowDetailLevel);
		const int ignoreSize = static_cast<int> (ignoreRatio * Graphics::getCurrentRenderTargetWidth ());

		//-- check minimum screen size
		//
		//   Estimated from distance and field of view rather than by projecting the object. This used
		//   to call computeRadiusInScreenSpace and test its result with &&, which meant a projection
		//   that FAILED skipped the cull entirely -- and projectInCameraSpace fails for anything
		//   outside the near or far plane. An object behind the camera therefore kept its shadow while
		//   the same object in front of the camera, small on screen, lost it. Turning the camera moved
		//   objects across that boundary, so shadows came and went with camera angle and the cost of
		//   building volumes for things the viewer could not see arrived during rotation.
		//
		//   This estimate is continuous and frustum independent: the same object at the same distance
		//   gets the same answer whichever way the camera faces, so there is no boundary to cross.
		{
			const float distance = sqrt (distanceSquared);
			const float tanHalfFov = tan (0.5f * camera.getHorizontalFieldOfView ());

			//-- Only applied past a quarter of the shadow distance.
			//
			//   A caster's own apparent size is a poor proxy for whether its shadow is visible: the
			//   volume extends up to ms_shadowVolumeExtrudeDistance from the caster, so something
			//   behind the camera, or small on screen, can still throw a shadow across the view. The
			//   shipped code got this right by accident -- it only size-culled objects it could
			//   project, and projection fails outside the near and far planes, so anything behind the
			//   camera escaped the cull. Making the test unconditional removed shadows that were
			//   being drawn for good reason, which shows up as shadows vanishing when you turn away
			//   from what casts them.
			//
			//   Near casters therefore keep their shadows whatever their screen size or direction.
			//   The size test still bounds the distant tail, where a shadow genuinely cannot reach
			//   back into view, and it is still computed from distance and field of view rather than
			//   by projecting -- so it stays continuous and there is nothing to flicker.
			const float sizeCullBeyond = 0.25f * maximumDistance;

			if (distance > sizeCullBeyond && distance > 0.01f && tanHalfFov > 0.f)
			{
				const float halfWidth = 0.5f * static_cast<float> (Graphics::getCurrentRenderTargetWidth ());
				const float screenRadius = (radius * halfWidth) / (distance * tanHalfFov);

				if (screenRadius < static_cast<float> (ignoreSize))
					return false;
			}
		}
		
		bool fogEnabled;
		float fogDensity;
		PackedArgb fogColor;
		Graphics::getFog(fogEnabled, fogDensity, fogColor);

		if(fogEnabled && fogDensity != 0.0f)
		{
			// Calculate our fog value
			float distance = camera.getPosition_w ().magnitudeBetween(position_w);
			float distanceDensitySqrd = pow(distance * fogDensity, 2);
			float baseDistanceDensity = pow(cms_fogNaturalBase, distanceDensitySqrd);
			float finalFogValue = 1.0f / baseDistanceDensity;

			const float fogCutOff = linearInterpolate (cms_maximumFogCutOff, cms_minimumFogCutOff, ms_shadowDetailLevel);

			if(finalFogValue < fogCutOff)
				return false;
		}

		return true;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	void debugReport ()
	{
		DEBUG_REPORT_PRINT (true, ("-- ShadowManager\n"));
		DEBUG_REPORT_PRINT (true, ("                   enabled = %s\n", ms_enabled ? "yes" : "no"));
		DEBUG_REPORT_PRINT (true, ("       shadow detail level = %1.2f\n", ms_shadowDetailLevel));
		DEBUG_REPORT_PRINT (true, ("    simple shadow distance = %1.2f\n", ms_simpleShadowDistance));
		DEBUG_REPORT_PRINT (true, ("volumetric shadow distance = %1.2f\n", ms_volumetricShadowDistance));
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
}

using namespace ShadowManagerNamespace;

// ======================================================================

void ShadowManager::install ()
{
	LocalMachineOptionManager::registerOption (ms_enabled, "ClientObject", "renderShadows");

	LocalMachineOptionManager::registerOption (ms_shadowDetailLevel, "ClientObject", "shadowDetailLevel");
	setShadowDetailLevel (ms_shadowDetailLevel);

	LocalMachineOptionManager::registerOption (ms_volumetricShadowDistanceLevel, "ClientObject", "volumetricShadowDistanceLevel");
	setVolumetricShadowDistanceLevel (ms_volumetricShadowDistanceLevel);

	LocalMachineOptionManager::registerOption (ms_meshShadows, "ClientObject", "meshShadows");
	LocalMachineOptionManager::registerOption (ms_skeletalShadows, "ClientObject", "skeletalShadows");

	updateShadows ();

	DebugFlags::registerFlag (ms_debugReport, "ClientObject", "reportShadowManager", debugReport);
	ExitChain::add (ShadowManagerNamespace::remove, "ShadowManagerNamespace::remove");
}

// ----------------------------------------------------------------------

bool ShadowManager::getEnabled ()
{
	return ms_enabled;
}

// ----------------------------------------------------------------------

void ShadowManager::setEnabled (bool const enabled)
{
	ms_enabled = enabled;
}

// ----------------------------------------------------------------------

bool ShadowManager::getEnabledDefault ()
{
	return true;
}

// ----------------------------------------------------------------------

bool ShadowManager::getAllowed()
{
	return ms_allowed;
}

// ----------------------------------------------------------------------

void ShadowManager::setAllowed(bool const allowed)
{
	ms_allowed = allowed;
}

// ----------------------------------------------------------------------

bool ShadowManager::getMeshShadowsNone ()
{
	return !ms_meshShadows;
}

// ----------------------------------------------------------------------

void ShadowManager::setMeshShadowsNone (bool const meshShadowsNone)
{
	if (meshShadowsNone)
		ms_meshShadows = false;

	updateShadows ();
}

// ----------------------------------------------------------------------

bool ShadowManager::getMeshShadowsNoneDefault () 
{ 
	return true; 
}

// ----------------------------------------------------------------------

bool ShadowManager::getMeshShadowsVolumetric ()
{
	return ms_meshShadows;
}

// ----------------------------------------------------------------------

void ShadowManager::setMeshShadowsVolumetric (bool const meshShadowsVolumetric)
{
	if (meshShadowsVolumetric)
		ms_meshShadows = true;

	updateShadows ();
}

// ----------------------------------------------------------------------

bool ShadowManager::getMeshShadowsVolumetricDefault () 
{ 
	return false; 
}

// ----------------------------------------------------------------------

bool ShadowManager::getSkeletalShadowsNone ()
{
	return ms_skeletalShadows == SST_none;
}

// ----------------------------------------------------------------------

bool ShadowManager::getSkeletalShadowsNoneDefault ()  
{ 
	return true; 
}

// ----------------------------------------------------------------------

void ShadowManager::setSkeletalShadowsNone (bool const skeletalShadowsNone)
{
	if (skeletalShadowsNone)
		ms_skeletalShadows = SST_none;

	updateShadows ();
}

// ----------------------------------------------------------------------

bool ShadowManager::getSkeletalShadowsSimple ()
{
	return ms_skeletalShadows == SST_simple;
}

// ----------------------------------------------------------------------

void ShadowManager::setSkeletalShadowsSimple (bool const skeletalShadowsSimple)
{
	if (skeletalShadowsSimple)
		ms_skeletalShadows = SST_simple;

	updateShadows ();
}

// ----------------------------------------------------------------------

bool ShadowManager::getSkeletalShadowsSimpleDefault ()  
{ 
	return true; 
}

// ----------------------------------------------------------------------

bool ShadowManager::getSkeletalShadowsVolumetric ()
{
	return ms_skeletalShadows == SST_volumetric;
}

// ----------------------------------------------------------------------

void ShadowManager::setSkeletalShadowsVolumetric (bool const skeletalShadowsVolumetric)
{
	if (skeletalShadowsVolumetric)
		ms_skeletalShadows = SST_volumetric;

	updateShadows ();
}

// ----------------------------------------------------------------------

bool ShadowManager::getSkeletalShadowsVolumetricDefault ()  
{ 
	return false; 
}

// ----------------------------------------------------------------------

void ShadowManager::setVolumetricShadowDistanceLevel (float const volumetricShadowDistanceLevel)
{
	ms_volumetricShadowDistanceLevel = clamp (0.f, volumetricShadowDistanceLevel, 1.f);
	ms_maximumVolumetricShadowDistance = linearInterpolate (cms_minimumVolumetricShadowDistance, cms_maximumVolumetricShadowDistance, ms_volumetricShadowDistanceLevel);

	setShadowDetailLevel (ms_shadowDetailLevel);
}

// ----------------------------------------------------------------------

float ShadowManager::getShadowDetailLevel ()
{
	return ms_shadowDetailLevel;
}

// ----------------------------------------------------------------------

void ShadowManager::setShadowDetailLevel (float const shadowDetailLevel)
{
	ms_shadowDetailLevel        = clamp (0.f, shadowDetailLevel, 1.f);
	ms_simpleShadowDistance     = linearInterpolate (cms_minimumSimpleShadowDistance, cms_maximumSimpleShadowDistance, ms_shadowDetailLevel);
	ms_volumetricShadowDistance = linearInterpolate (cms_minimumVolumetricShadowDistance, ms_maximumVolumetricShadowDistance, ms_shadowDetailLevel);
}

// ----------------------------------------------------------------------

float ShadowManager::getShadowDetailLevelDefault ()
{
	return 0.f;
}

// ----------------------------------------------------------------------

float ShadowManager::getSimpleShadowDistance ()
{
	return ms_simpleShadowDistance;
}

// ----------------------------------------------------------------------

bool ShadowManager::simpleShouldRender (Camera const & camera, Vector const & position_w, float const radius)
{
	return ms_enabled && shouldRender (camera, position_w, radius, ShadowManager::getSimpleShadowDistance ());
}

// ----------------------------------------------------------------------

float ShadowManager::getVolumetricShadowDistance ()
{
	return ms_volumetricShadowDistance;
}

// ----------------------------------------------------------------------

bool ShadowManager::volumetricShouldRender (Camera const & camera, Vector const & position_w, float const radius)
{
	return ms_enabled && shouldRender (camera, position_w, radius, ShadowManager::getVolumetricShadowDistance ());
}

// ----------------------------------------------------------------------

float ShadowManager::getTimeOfDay()
{
	return ms_timeOfDay;
}

// ----------------------------------------------------------------------

void ShadowManager::setTimeOfDay(float time)
{
	ms_timeOfDay = time;
}

// ======================================================================
