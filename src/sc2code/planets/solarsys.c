//Copyright Paul Reiche, Fred Ford. 1992-2002

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "starcon.h"
#include "libs/graphics/gfx_common.h"

//Added by Chris

void LoadLanderData (void);

void RenderTopography (BOOLEAN Reconstruct);

//End Added by Chris

PSOLARSYS_STATE pSolarSysState;
FRAME SISIPFrame, SunFrame, OrbitalFrame, SpaceJunkFrame;
COLORMAP OrbitalCMap;
MUSIC_REF SpaceMusic;

#define DRAW_STARS (1 << 0)
#define DRAW_PLANETS (1 << 1)
#define DRAW_ORBITS (1 << 2)
#define DRAW_HYPER_COORDS (1 << 3)
#define UNBATCH_SYS (1 << 4)
#define DRAW_REFRESH (1 << 5)
#define REPAIR_SCAN (1 << 6)
#define GRAB_BKGND (1 << 7)

static SIZE old_radius;
BYTE draw_sys_flags = DRAW_STARS
								| DRAW_PLANETS
								| DRAW_ORBITS
								| DRAW_HYPER_COORDS
								| GRAB_BKGND;

static void
GenerateMoons (void)
{
	COUNT i, facing;
	PPLANET_DESC pMoonDesc;
	PPLANET_DESC pCurDesc;
	DWORD old_seed;

	GLOBAL (ip_location.x) =
			(SIZE)((long)(GLOBAL (ShipStamp.origin.x)
			- (SIS_SCREEN_WIDTH >> 1))
			* MAX_ZOOM_RADIUS / (DISPLAY_FACTOR >> 1));
	GLOBAL (ip_location.y) =
			(SIZE)((long)(GLOBAL (ShipStamp.origin.y)
			- (SIS_SCREEN_HEIGHT >> 1))
			* MAX_ZOOM_RADIUS / (DISPLAY_FACTOR >> 1));
	pCurDesc = pSolarSysState->pBaseDesc;
	old_seed = TFB_SeedRandom (pCurDesc->rand_seed);

	(*pSolarSysState->GenFunc) (GENERATE_NAME);
	(*pSolarSysState->GenFunc) (GENERATE_MOONS);

	facing = NORMALIZE_FACING (ANGLE_TO_FACING (
			ARCTAN (pCurDesc->location.x, pCurDesc->location.y)
			));
	for (i = 0, pMoonDesc = &pSolarSysState->MoonDesc[0];
			i < MAX_MOONS; ++i, ++pMoonDesc)
	{
		pMoonDesc->pPrevDesc = pCurDesc;
		if (pSolarSysState->MenuState.Initialized > 1
				|| i >= pCurDesc->NumPlanets)
			;
		else
		{
			BYTE data_index;

			pMoonDesc->temp_color = pCurDesc->temp_color;

			data_index = pMoonDesc->data_index;
					/* Starbase */
			if (data_index == (BYTE)~0)
			{
				pMoonDesc->image.frame = SetAbsFrameIndex (
						SpaceJunkFrame, 16
						);
			}
					/* Samatra */
			else if (data_index == (BYTE)(~0 - 1))
			{
				pMoonDesc->image.frame = SetAbsFrameIndex (
						SpaceJunkFrame, 19
						);
			}
		}
	}

	pSolarSysState->pBaseDesc = pSolarSysState->MoonDesc;
	TFB_SeedRandom (old_seed);
}

void
FreeIPData (void)
{
	DestroyDrawable (ReleaseDrawable (SISIPFrame));
	SISIPFrame = 0;
	DestroyDrawable (ReleaseDrawable (SunFrame));
	SunFrame = 0;
	DestroyColorMap (ReleaseColorMap (OrbitalCMap));
	OrbitalCMap = 0;
	DestroyDrawable (ReleaseDrawable (OrbitalFrame));
	OrbitalFrame = 0;
	DestroyDrawable (ReleaseDrawable (SpaceJunkFrame));
	SpaceJunkFrame = 0;
	DestroyMusic (SpaceMusic);
	SpaceMusic = 0;
}

void
LoadIPData (void)
{
	if (SpaceJunkFrame == 0)
	{
		SpaceJunkFrame = CaptureDrawable (
				LoadGraphic (IPBKGND_MASK_PMAP_ANIM)
				);
		SISIPFrame =
				CaptureDrawable (
				LoadGraphic (SISIP_MASK_PMAP_ANIM)
				);

		OrbitalCMap = CaptureColorMap (LoadColorMap (ORBPLAN_COLOR_MAP));
		OrbitalFrame = CaptureDrawable (
				LoadGraphic (ORBPLAN_MASK_PMAP_ANIM)
				);
		SunFrame = CaptureDrawable (LoadGraphic (SUN_MASK_PMAP_ANIM));

		SpaceMusic = LoadMusicInstance (IP_MUSIC);
	}
}

void
LoadSolarSys (void)
{
	COUNT i;
	PPLANET_DESC pCurDesc;
	DWORD old_seed;
#define NUM_TEMP_RANGES 5
	COLOR temp_color_array[NUM_TEMP_RANGES] =
	{
		BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0xE), 0x54),
		BUILD_COLOR (MAKE_RGB15 (0x00, 0x6, 0x8), 0x62),
		BUILD_COLOR (MAKE_RGB15 (0x00, 0xB, 0x00), 0x6D),
		BUILD_COLOR (MAKE_RGB15 (0xF, 0x00, 0x00), 0x2D),
		BUILD_COLOR (MAKE_RGB15 (0xF, 0x8, 0x00), 0x75),
	};

	pSolarSysState->MenuState.CurState = 0;
	pSolarSysState->MenuState.Initialized =
			HIBYTE (HIWORD (GLOBAL (ShipStamp.frame)));
	if (pSolarSysState->MenuState.Initialized)
	{
		GLOBAL (ShipStamp.frame) = (FRAME)MAKE_DWORD (
				LOWORD (GLOBAL (ShipStamp.frame)),
				LOBYTE (HIWORD (GLOBAL (ShipStamp.frame)))
				);
		++pSolarSysState->MenuState.Initialized;
	}
	else
	{
// LoadIPData ();
	}

	old_seed = TFB_SeedRandom (
			MAKE_DWORD (CurStarDescPtr->star_pt.x,
			CurStarDescPtr->star_pt.y)
			);

	SunFrame = SetAbsFrameIndex (SunFrame,
			STAR_TYPE (CurStarDescPtr->Type)
			);

	pCurDesc = &pSolarSysState->SunDesc[0];
	pCurDesc->pPrevDesc = 0;
	pCurDesc->rand_seed = TFB_Random ();

	pCurDesc->data_index = STAR_TYPE (CurStarDescPtr->Type);
	pCurDesc->location.x = pCurDesc->location.y = 0;
	pCurDesc->image.origin = pCurDesc->location;
	pCurDesc->image.frame = SunFrame;

	(*pSolarSysState->GenFunc) (GENERATE_PLANETS);
	if (GET_GAME_STATE (PLANETARY_CHANGE))
	{
		PutPlanetInfo ();
		SET_GAME_STATE (PLANETARY_CHANGE, 0);
	}

	for (i = 0, pCurDesc = pSolarSysState->PlanetDesc;
			i < MAX_PLANETS; ++i, ++pCurDesc)
	{
		pCurDesc->pPrevDesc = &pSolarSysState->SunDesc[0];
		pCurDesc->image.origin = pCurDesc->location;
		if (pSolarSysState->MenuState.Initialized != 0
				|| i >= pSolarSysState->SunDesc[0].NumPlanets)
			pCurDesc->image.frame = 0;
		else
		{
			COUNT index;
			SYSTEM_INFO SysInfo;

			DoPlanetaryAnalysis (&SysInfo, pCurDesc);
			index = (SysInfo.PlanetInfo.SurfaceTemperature + 250) / 100;
			if (index >= NUM_TEMP_RANGES)
				index = NUM_TEMP_RANGES - 1;
			pCurDesc->temp_color = temp_color_array[index];
		}
	}

	{
		SIZE sort_array[MAX_PLANETS + 1];

		// When this part is done, sort_array will contain the indices to
		// all planets, sorted on their y position.
		// The sun itself, which has its data located at
		// pSolarSysState->PlanetDesc[-1], is included in this array. 
		// Very ugly stuff, but it's correct.

		for (i = 0; i <= pSolarSysState->SunDesc[0].NumPlanets; ++i)
			sort_array[i] = i - 1;

		for (i = 0; i <= pSolarSysState->SunDesc[0].NumPlanets; ++i)
		{
			COUNT j;

			for (j = pSolarSysState->SunDesc[0].NumPlanets; j > i; --j)
			{
				SIZE real_i, real_j;

				real_i = sort_array[i];
				real_j = sort_array[j];
				if (pSolarSysState->PlanetDesc[real_i].image.origin.y >
						pSolarSysState->PlanetDesc[real_j].image.origin.y)
				{
					SIZE temp;

					temp = sort_array[i];
					sort_array[i] = sort_array[j];
					sort_array[j] = temp;
				}
			}
		}

		pSolarSysState->FirstPlanetIndex = sort_array[0];
		pSolarSysState->LastPlanetIndex = sort_array[
				pSolarSysState->SunDesc[0].NumPlanets
				];
		for (i = 0; i <= pSolarSysState->SunDesc[0].NumPlanets; ++i)
			pSolarSysState->PlanetDesc[sort_array[i]].NextIndex =
					sort_array[i + 1];
	}

	if ((i = LOBYTE (HIWORD (GLOBAL (ShipStamp.frame)))) == 0)
		pSolarSysState->pBaseDesc =
				pSolarSysState->pOrbitalDesc = pSolarSysState->PlanetDesc;
	else
	{
		pSolarSysState->pOrbitalDesc = 0;
		pSolarSysState->pBaseDesc = &pSolarSysState->PlanetDesc[i - 1];
		pSolarSysState->SunDesc[0].location =
				GLOBAL (ip_location);
		GenerateMoons ();

		SET_GAME_STATE (PLANETARY_LANDING, 0);
	}

	{
		BYTE i, num_thrusters;

		num_thrusters = 0;
		for (i = 0; i < NUM_DRIVE_SLOTS; ++i)
		{
			if (GLOBAL_SIS (DriveSlots[i]) == FUSION_THRUSTER)
				++num_thrusters;
		}
		pSolarSysState->max_ship_speed = (BYTE)(
				(num_thrusters + 5) * IP_SHIP_THRUST_INCREMENT
				);

		pSolarSysState->turn_wait = IP_SHIP_TURN_WAIT;
		for (i = 0; i < NUM_JET_SLOTS; ++i)
		{
			if (GLOBAL_SIS (JetSlots[i]) == TURNING_JETS)
				pSolarSysState->turn_wait -= IP_SHIP_TURN_DECREMENT;
		}
	}

	i = pSolarSysState->MenuState.Initialized;
	if (i)
	{
		if ((i -= 2) == 0)
			pSolarSysState->pOrbitalDesc = pSolarSysState->pBaseDesc->pPrevDesc;
		else
			pSolarSysState->pOrbitalDesc = &pSolarSysState->MoonDesc[i - 1];
		pSolarSysState->MenuState.Initialized = 2;
		GLOBAL (ip_location) =
				pSolarSysState->SunDesc[0].location;
	}
	else
	{
		if ((i = LOWORD (GLOBAL (ShipStamp.frame))) == 0)
			++i;

		GLOBAL (ShipStamp.frame) =
				SetAbsFrameIndex (SISIPFrame, i - 1);
	}

	TFB_SeedRandom (old_seed);
}

static void
FreeSolarSys (void)
{
	if (pSolarSysState->MenuState.flash_task)
	{
		if (pSolarSysState->MenuState.Initialized >= 3)
			FreePlanet ();
		else
		{			
			ConcludeTask (pSolarSysState->MenuState.flash_task);
			pSolarSysState->MenuState.flash_task = 0;
			SetSemaphore (GraphicsSem);
			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				extern void SaveFlagshipState (void);

				SaveFlagshipState ();
			}
			ClearSemaphore (GraphicsSem);
		}
	}

	SetSemaphore (GraphicsSem);

	SetContext (SpaceContext);
	SetContextBGFrame ((FRAME)0);

	StopMusic ();

//    FreeIPData ();

	ClearSemaphore (GraphicsSem);
}

static void
CheckIntersect (BOOLEAN just_checking)
{
	COUNT i;
	PPLANET_DESC pCurDesc;
	INTERSECT_CONTROL ShipIntersect, PlanetIntersect;
	COUNT NewWaitPlanet;
	BYTE PlanetOffset, MoonOffset;

	pCurDesc = pSolarSysState->pBaseDesc->pPrevDesc;

	ShipIntersect.IntersectStamp.origin = GLOBAL (ShipStamp.origin);
	ShipIntersect.EndPoint = ShipIntersect.IntersectStamp.origin;
	ShipIntersect.IntersectStamp.frame = GLOBAL (ShipStamp.frame);

	PlanetIntersect.IntersectStamp.origin.x = SIS_SCREEN_WIDTH >> 1;
	PlanetIntersect.IntersectStamp.origin.y = SIS_SCREEN_HEIGHT >> 1;
	PlanetIntersect.EndPoint = PlanetIntersect.IntersectStamp.origin;
	if (pSolarSysState->WaitIntersect != (COUNT)~0
			&& pSolarSysState->WaitIntersect != MAKE_WORD (
					pCurDesc - pSolarSysState->PlanetDesc + 1, 1
					))
		PlanetIntersect.IntersectStamp.frame = DecFrameIndex (stars_in_space);
	else
		PlanetIntersect.IntersectStamp.frame = pCurDesc->image.frame;

	NewWaitPlanet = 0;

	if (pCurDesc != pSolarSysState->SunDesc /* can't intersect with sun */
			&& DrawablesIntersect (&ShipIntersect,
			&PlanetIntersect, MAX_TIME_VALUE))
	{
		PlanetOffset = pCurDesc - pSolarSysState->PlanetDesc + 1;
		MoonOffset = 1;
//fprintf (stderr, "0: Planet %d, Moon %d\n", PlanetOffset, MoonOffset);
		NewWaitPlanet = MAKE_WORD (PlanetOffset, MoonOffset);
		if (pSolarSysState->WaitIntersect != (COUNT)~0
				&& pSolarSysState->WaitIntersect != NewWaitPlanet)
		{
ShowPlanet:
			pSolarSysState->WaitIntersect = NewWaitPlanet;
		
			if (!just_checking)
			{
				ZeroVelocityComponents (&GLOBAL (velocity));
				++pSolarSysState->MenuState.Initialized;
				pSolarSysState->pOrbitalDesc = pCurDesc;
			}

#ifdef DEBUG
			fprintf (stderr, "Star index = %d, Planet index = %d, <%d, %d>\n",
					CurStarDescPtr - star_array,
					pCurDesc - pSolarSysState->PlanetDesc,
					pSolarSysState->SunDesc[0].location.x,
					pSolarSysState->SunDesc[0].location.y);
#endif /* DEBUG */
			return;
		}
	}

	{
		for (i = pCurDesc->NumPlanets,
				pCurDesc = pSolarSysState->pBaseDesc; i; --i, ++pCurDesc)
		{
			PlanetIntersect.IntersectStamp.origin = pCurDesc->image.origin;
			PlanetIntersect.EndPoint = PlanetIntersect.IntersectStamp.origin;
			if (pSolarSysState->pBaseDesc == pSolarSysState->MoonDesc)
			{
				PlanetOffset = pCurDesc->pPrevDesc - pSolarSysState->PlanetDesc;
				MoonOffset = pCurDesc - pSolarSysState->MoonDesc + 2;
			}
			else
			{
				PlanetOffset = pCurDesc - pSolarSysState->PlanetDesc;
				MoonOffset = 0;
			}
			++PlanetOffset;
			if (pSolarSysState->WaitIntersect != (COUNT)~0
					&& pSolarSysState->WaitIntersect != MAKE_WORD (PlanetOffset, MoonOffset))
				PlanetIntersect.IntersectStamp.frame = DecFrameIndex (stars_in_space);
			else
				PlanetIntersect.IntersectStamp.frame = pCurDesc->image.frame;

			if (DrawablesIntersect (&ShipIntersect,
					&PlanetIntersect, MAX_TIME_VALUE))
			{
//				fprintf (stderr, "1: Planet %d, Moon %d\n", PlanetOffset, MoonOffset);
				NewWaitPlanet = MAKE_WORD (PlanetOffset, MoonOffset);
				
				if (pSolarSysState->WaitIntersect == (COUNT)~0)
					return;
				else if (pSolarSysState->WaitIntersect == NewWaitPlanet)
					continue;
				else if (pSolarSysState->pBaseDesc == pSolarSysState->MoonDesc)
					goto ShowPlanet;
				else if (!just_checking) /* pBaseDesc == PlanetDesc */
				{
					COUNT angle;
					RECT r;

					angle = FACING_TO_ANGLE (
							GetFrameIndex (GLOBAL (ShipStamp.frame))
							) + HALF_CIRCLE;
					GLOBAL (ShipStamp.origin.x) =
							(SIS_SCREEN_WIDTH >> 1) + COSINE (
							angle, MIN_MOON_RADIUS
							+ ((MAX_MOONS - 1) * MOON_DELTA)
							+ (MOON_DELTA >> 2)
							);
					GLOBAL (ShipStamp.origin.y) =
							(SIS_SCREEN_HEIGHT >> 1) + SINE (
							angle, MIN_MOON_RADIUS
							+ ((MAX_MOONS - 1) * MOON_DELTA)
							+ (MOON_DELTA >> 2)
							);
					if (GLOBAL (ShipStamp.origin.y) < 0)
						GLOBAL (ShipStamp.origin.y) = 1;
					else if (GLOBAL (ShipStamp.origin.y) >= SIS_SCREEN_HEIGHT)
						GLOBAL (ShipStamp.origin.y) =
								(SIS_SCREEN_HEIGHT - 1) - 1;
					pSolarSysState->pBaseDesc = pCurDesc;
					XFormIPLoc (
							&pCurDesc->image.origin,
							&pSolarSysState->SunDesc[0].location,
							FALSE
							);
					ZeroVelocityComponents (&GLOBAL (velocity));
					GenerateMoons ();

					NewWaitPlanet = 0;
					BatchGraphics ();
					SetGraphicGrabOther (1);
					DrawSystem (pSolarSysState->pBaseDesc->pPrevDesc->radius, TRUE);
					SetGraphicGrabOther (0);
					r.corner.x = SIS_ORG_X;
					r.corner.y = SIS_ORG_Y;
					r.extent.width = SIS_SCREEN_WIDTH;
					r.extent.height = SIS_SCREEN_HEIGHT;
					ScreenTransition (3, &r);
					UnbatchGraphics ();
					LoadIntoExtraScreen (&r);
				}
				break;
			}
		}
	}

	if (pSolarSysState->WaitIntersect != (COUNT)~0 || NewWaitPlanet == 0)
		pSolarSysState->WaitIntersect = NewWaitPlanet;
}

static void
GetOrbitRect (PRECT pRect, COORD cx, COORD cy, SIZE
		radius, COUNT xnumer, COUNT ynumer, COUNT denom)
{
#ifdef BVT_NOT
	pRect->corner.x = (SIS_SCREEN_WIDTH >> 1)
			+ (SIZE)((long)-cx * xnumer / denom);
	pRect->corner.y = (SIS_SCREEN_HEIGHT >> 1)
			+ (SIZE)((long)-cy * ynumer / denom);
	pRect->extent.width = (SIZE)(radius * (DWORD)(xnumer << 1) / denom);
#endif
	pRect->corner.x = (SIS_SCREEN_WIDTH >> 1)
			+ (SIZE)((long)-cx * (long)xnumer / (long)denom);
	pRect->corner.y = (SIS_SCREEN_HEIGHT >> 1)
			+ (SIZE)((long)-cy * (long)ynumer / (long)denom);
	pRect->extent.width = (SIZE)((long)radius * (long)(xnumer << 1) / (long)denom);
	pRect->extent.height = pRect->extent.width >> 1;
}

static void
DrawOrbit (PPLANET_DESC pPlanetDesc, COUNT xnumer, COUNT
		ynumer0, COUNT ynumer1, COUNT denom)
{
	COUNT index;
	COORD cx, cy;
	RECT r;

if (!(draw_sys_flags & (DRAW_ORBITS | DRAW_PLANETS)))
	return;
	
	cx = cy = pPlanetDesc->radius;
	if (xnumer > (COUNT)DISPLAY_FACTOR)
	{
		cx = cx + pPlanetDesc->location.x;
		cy = (cy + pPlanetDesc->location.y) << 1;
	}
	GetOrbitRect (&r, cx, cy,
			pPlanetDesc->radius, xnumer, ynumer0, denom);

if (draw_sys_flags & DRAW_ORBITS)
{
	if (pSolarSysState->pBaseDesc)
	{
		SetContextForeGroundColor (pPlanetDesc->temp_color);
		DrawOval (&r, 1);
	}
}

if (!(draw_sys_flags & DRAW_PLANETS))
	return;
	
	r.corner.x += (r.extent.width >> 1);
	r.corner.y += (r.extent.height >> 1);
	r.corner.x = r.corner.x
			+ (SIZE)((long)pPlanetDesc->location.x
			* (long)xnumer / (long)denom);
	r.corner.y = r.corner.y
			+ (SIZE)((long)pPlanetDesc->location.y
			* (long)ynumer1 / (long)denom);

	index = pPlanetDesc->data_index & ~PLANET_SHIELDED;
	if (index < NUMBER_OF_PLANET_TYPES)
	{
		BYTE Type;
		COUNT Size;

		Type = PlanData[index].Type;
		SetColorMap (GetColorMapAddress (
				SetAbsColorMapIndex (OrbitalCMap,
				PLANCOLOR (Type))
				));

		Size = PLANSIZE (Type);
		if (denom == 2 || xnumer > (COUNT)DISPLAY_FACTOR)
			Size += 3;
		else if (denom <= (COUNT)(MAX_ZOOM_RADIUS >> 2))
		{
			++Size;
			if (denom == MIN_ZOOM_RADIUS)
				++Size;
		}
		if (pPlanetDesc->pPrevDesc == &pSolarSysState->SunDesc[0])
			pPlanetDesc->image.frame =
					SetAbsFrameIndex (OrbitalFrame,
					(Size << FACING_SHIFT)
					+ NORMALIZE_FACING (ANGLE_TO_FACING (
					ARCTAN (pPlanetDesc->location.x, pPlanetDesc->location.y)
					)));
		else
		{
			--Size;
			pPlanetDesc->image.frame =
					SetAbsFrameIndex (OrbitalFrame,
					(Size << FACING_SHIFT)
					+ NORMALIZE_FACING (ANGLE_TO_FACING (
					ARCTAN (
							pPlanetDesc->pPrevDesc->location.x,
							pPlanetDesc->pPrevDesc->location.y
							)
					)));
		}
	}

	if (!(denom == 2 || xnumer > (COUNT)DISPLAY_FACTOR))
	{
		pPlanetDesc->image.origin = r.corner;
	}
	else
	{
		STAMP s;

		if (denom == 2)
			pPlanetDesc->image.origin = r.corner;

		s.origin = r.corner;
		s.frame = pPlanetDesc->image.frame;
		DrawStamp (&s);
		if (index < NUMBER_OF_PLANET_TYPES
				&& (pPlanetDesc->data_index & PLANET_SHIELDED))
		{
			s.frame = SetAbsFrameIndex (SpaceJunkFrame, 17);
			DrawStamp (&s);
		}
	}
}

static void
FindRadius (void)
{
	SIZE delta_x, delta_y;
	SIZE radius;

	do
	{
		if ((pSolarSysState->SunDesc[0].radius >>= 1) > MIN_ZOOM_RADIUS)
			radius = pSolarSysState->SunDesc[0].radius >> 1;
		else
			radius = 0;

		GetOrbitRect (&pSolarSysState->MenuState.flash_rect0,
				radius, radius, radius,
				DISPLAY_FACTOR,
				DISPLAY_FACTOR >> 2,
				pSolarSysState->SunDesc[0].radius);

		XFormIPLoc (&GLOBAL (ip_location),
				&GLOBAL (ShipStamp.origin),
				TRUE);

	} while (radius
			&& (delta_x = GLOBAL (ShipStamp.origin.x)
			- pSolarSysState->MenuState.flash_rect0.corner.x) >= 0
			&& (delta_y = GLOBAL (ShipStamp.origin.y)
			- pSolarSysState->MenuState.flash_rect0.corner.y) >= 0
			&& delta_x < pSolarSysState->MenuState.flash_rect0.extent.width
			&& delta_y < pSolarSysState->MenuState.flash_rect0.extent.height);
}

void
ZoomSystem (void)
{
	RECT r;

	BatchGraphics ();
	if (pSolarSysState->pBaseDesc == pSolarSysState->MoonDesc)
		DrawSystem (pSolarSysState->pBaseDesc->pPrevDesc->radius, TRUE);
	else
	{
		if (pSolarSysState->MenuState.CurState == 0)
			FindRadius ();

		DrawSystem (pSolarSysState->SunDesc[0].radius, FALSE);
	}
	r.corner.x = SIS_ORG_X;
	r.corner.y = SIS_ORG_Y;
	r.extent.width = SIS_SCREEN_WIDTH;
	r.extent.height = SIS_SCREEN_HEIGHT;
	ScreenTransition (3, &r);
	UnbatchGraphics ();
	LoadIntoExtraScreen (&r);
}

static UWORD
flagship_inertial_thrust (register COUNT CurrentAngle)
{
	BYTE max_speed;
	SIZE cur_delta_x, cur_delta_y;
	register COUNT TravelAngle;
	register VELOCITYPTR VelocityPtr;

	max_speed = pSolarSysState->max_ship_speed;
	VelocityPtr = &GLOBAL (velocity);
	GetCurrentVelocityComponents (VelocityPtr, &cur_delta_x, &cur_delta_y);
	if ((TravelAngle =
			GetVelocityTravelAngle (VelocityPtr)) == CurrentAngle
			&& cur_delta_x == COSINE (CurrentAngle, max_speed)
			&& cur_delta_y == SINE (CurrentAngle, max_speed))
		return (SHIP_AT_MAX_SPEED);
	else
	{
		register SIZE delta_x, delta_y;
		DWORD desired_speed;

		delta_x = cur_delta_x
				+ COSINE (CurrentAngle, IP_SHIP_THRUST_INCREMENT);
		delta_y = cur_delta_y
				+ SINE (CurrentAngle, IP_SHIP_THRUST_INCREMENT);
		desired_speed = (DWORD) ((long) delta_x * delta_x)
				+ (DWORD) ((long) delta_y * delta_y);
		if (desired_speed <= (DWORD) ((UWORD) max_speed * max_speed))
			SetVelocityComponents (VelocityPtr, delta_x, delta_y);
		else if (TravelAngle == CurrentAngle)
		{
			SetVelocityComponents (VelocityPtr,
					COSINE (CurrentAngle, max_speed),
					SINE (CurrentAngle, max_speed));
			return (SHIP_AT_MAX_SPEED);
		}
		else
		{
			VELOCITY_DESC v;

			v = *VelocityPtr;

			DeltaVelocityComponents (&v,
					COSINE (CurrentAngle, IP_SHIP_THRUST_INCREMENT >> 1)
					- COSINE (TravelAngle, IP_SHIP_THRUST_INCREMENT),
					SINE (CurrentAngle, IP_SHIP_THRUST_INCREMENT >> 1)
					- SINE (TravelAngle, IP_SHIP_THRUST_INCREMENT));
			GetCurrentVelocityComponents (&v, &cur_delta_x, &cur_delta_y);
			desired_speed =
					(DWORD) ((long) cur_delta_x * cur_delta_x)
					+ (DWORD) ((long) cur_delta_y * cur_delta_y);
			if (desired_speed > (DWORD) ((UWORD) max_speed * max_speed))
			{
				SetVelocityComponents (VelocityPtr,
						COSINE (CurrentAngle, max_speed),
						SINE (CurrentAngle, max_speed));
				return (SHIP_AT_MAX_SPEED);
			}

			*VelocityPtr = v;
		}

		return (0);
	}
}

extern void DrawIPRadar (BOOLEAN FirstTime);

static void
UndrawShip (INPUT_STATE InputState)
{
	COUNT index;
	SIZE radius, delta_x, delta_y;
	BOOLEAN LeavingInnerSystem;

	ClockTick ();

#ifdef SHOW_RADAR
	DrawIPRadar (FALSE);
#endif /* SHOW_RADAR */

	delta_x = GetInputXComponent (InputState);
//    delta_y = GetInputYComponent (InputState);
	if (InputState & DEVICE_BUTTON1)
		delta_y = -1;
	else
			delta_y = GetInputYComponent (InputState);

	if (delta_x || delta_y < 0)
	{
		GLOBAL (autopilot.x) =
				GLOBAL (autopilot.y) = ~0;
	}
	else if (GLOBAL (autopilot.x) != ~0
			&& GLOBAL (autopilot.y) != ~0)
		delta_y = -1;
	else
		delta_y = 0;

	index = GetFrameIndex (GLOBAL (ShipStamp.frame));
	if (pSolarSysState->turn_counter)
		--pSolarSysState->turn_counter;
	else if (delta_x)
	{
		if (delta_x < 0)
			index = NORMALIZE_FACING (index - 1);
		else
			index = NORMALIZE_FACING (index + 1);

		GLOBAL (ShipStamp.frame) =
				SetAbsFrameIndex (GLOBAL (ShipStamp.frame),
				index);

		pSolarSysState->turn_counter = pSolarSysState->turn_wait;
	}
	if (pSolarSysState->thrust_counter)
		--pSolarSysState->thrust_counter;
	else if (delta_y < 0)
	{
#define THRUST_WAIT 1
		flagship_inertial_thrust (FACING_TO_ANGLE (index));

		pSolarSysState->thrust_counter = THRUST_WAIT;
	}

	LeavingInnerSystem = FALSE;
	radius = pSolarSysState->SunDesc[0].radius;
	if (GLOBAL (ShipStamp.origin.x) < 0
			|| GLOBAL (ShipStamp.origin.x) >= SIS_SCREEN_WIDTH
			|| GLOBAL (ShipStamp.origin.y) < 0
			|| GLOBAL (ShipStamp.origin.y) >= SIS_SCREEN_HEIGHT)
	{
		if (pSolarSysState->pBaseDesc == pSolarSysState->PlanetDesc)
		{
			if (radius == MAX_ZOOM_RADIUS)
			{
				GLOBAL (CurrentActivity) |= END_INTERPLANETARY;
				return;
			}
		}
		else
		{
			PPLANET_DESC pPlanetDesc;

			LeavingInnerSystem = TRUE;
			pPlanetDesc = pSolarSysState->pBaseDesc->pPrevDesc;
			pSolarSysState->pBaseDesc = pSolarSysState->PlanetDesc;

			pSolarSysState->WaitIntersect = MAKE_WORD (
					pPlanetDesc - pSolarSysState->PlanetDesc + 1, 0
					);
			GLOBAL (ip_location) =
					pSolarSysState->SunDesc[0].location;
			ZeroVelocityComponents (&GLOBAL (velocity));
		}

		radius = MAX_ZOOM_RADIUS << 1;
	}

	if (pSolarSysState->pBaseDesc == pSolarSysState->PlanetDesc
			&& (radius > MAX_ZOOM_RADIUS
			|| ((delta_x = GLOBAL (ShipStamp.origin.x)
			- pSolarSysState->MenuState.flash_rect0.corner.x) >= 0
			&& (delta_y = GLOBAL (ShipStamp.origin.y)
			- pSolarSysState->MenuState.flash_rect0.corner.y) >= 0
			&& delta_x < pSolarSysState->MenuState.flash_rect0.extent.width
			&& delta_y < pSolarSysState->MenuState.flash_rect0.extent.height)))
	{
		old_radius = pSolarSysState->SunDesc[0].radius;
		pSolarSysState->SunDesc[0].radius = radius;
		FindRadius ();
		if (old_radius == (MAX_ZOOM_RADIUS << 1)
				|| old_radius == pSolarSysState->SunDesc[0].radius
				|| LeavingInnerSystem)
		{
			old_radius = 0;
			if (LeavingInnerSystem)
				SetGraphicGrabOther (1);
			DrawSystem (pSolarSysState->SunDesc[0].radius, FALSE);
			if (LeavingInnerSystem)
			{
				COUNT OldWI;

				SetGraphicGrabOther (0);
				OldWI = pSolarSysState->WaitIntersect;
				CheckIntersect (TRUE);
				if (pSolarSysState->WaitIntersect != OldWI)
				{
					pSolarSysState->WaitIntersect = (COUNT)~0;
					return;
				}
			}
		}
		else
			return;
	}

	if (GLOBAL (autopilot.x) == ~0
			&& GLOBAL (autopilot.y) == ~0)
		CheckIntersect (FALSE);
}

#if 0
static void
DrawSimpleSystem (SIZE radius, BYTE flags)
{
	draw_sys_flags &= ~flags;
	DrawSystem (radius, FALSE);
	draw_sys_flags |= flags;
}
#endif

static void
ScaleSystem (void)
{
#if 0
#define NUM_STEPS 8
	COUNT num_steps;
	SIZE err, d, new_radius, step;
	RECT r;
	BOOLEAN first_time;
	CONTEXT OldContext;

	first_time = TRUE;
	new_radius = pSolarSysState->SunDesc[0].radius;
	
	BatchGraphics ();
	DrawSimpleSystem (new_radius, DRAW_PLANETS | DRAW_ORBITS | GRAB_BKGND);

	pSolarSysState->SunDesc[0].radius = old_radius;
	
	d = new_radius - old_radius;
	step = d / NUM_STEPS;
	if (d < 0)
		d = -d;

	num_steps = err = NUM_STEPS;

	OldContext = SetContext (SpaceContext);
	GetContextClipRect (&r);
	SetGraphicGrabOther (1); // to grab from hidden screen (since we haven't flipped yet)
	LoadIntoExtraScreen (&r);
	SetContextBGFrame ((FRAME)0);
	SetGraphicGrabOther (0);
	SetContextFGFrame (Screen);

	do
	{
		if ((err -= d) <= 0)
		{
			pSolarSysState->SunDesc[0].radius += step;

			BatchGraphics ();

			DrawFromExtraScreen (r.corner.x, r.corner.y);

			DrawSimpleSystem (pSolarSysState->SunDesc[0].radius,
					DRAW_ORBITS | DRAW_STARS | GRAB_BKGND | DRAW_HYPER_COORDS);
					
			RedrawQueue (FALSE);
			
			if (first_time)
			{
				first_time = FALSE;
				UnbatchGraphics (); // to balance out Batch before DrawSimpleSystem above
			}

			UnbatchGraphics ();
			
			err += d;
		}
	} while (--num_steps);
	
	SetContext (OldContext);
	pSolarSysState->SunDesc[0].radius = new_radius;
	DrawSystem (pSolarSysState->SunDesc[0].radius, FALSE);
	old_radius = 0;
#else
	RECT r;
	CONTEXT OldContext;
	
	OldContext = SetContext (SpaceContext);
	BatchGraphics ();
	DrawSystem (pSolarSysState->SunDesc[0].radius, FALSE);
	GetContextClipRect (&r);
	ScreenTransition (3, &r);
	UnbatchGraphics ();
	LoadIntoExtraScreen (&r);
	SetContext (OldContext);
	
	old_radius = 0;
#endif
}

int IPtask_func(void* data)
{
#define IP_FRAME_RATE  (ONE_SECOND / 30)
#define DEBOUNCE_DELAY ((ONE_SECOND >> 1) / IP_FRAME_RATE)
	BYTE MenuTransition;
	DWORD NextTime;
	INPUT_STATE InputState;
	Task task = (Task) data;

	TaskSwitch ();

	if (LastActivity != CHECK_LOAD)
		InputState = 0;
	else
		InputState = DEVICE_BUTTON2;

	MenuTransition = 0;
	NextTime = GetTimeCounter ();
	while (!Task_ReadState (task, TASK_EXIT))
	{
		CONTEXT OldContext;
		BOOLEAN InnerSystem;
		RECT r;

		InnerSystem = FALSE;
		SetSemaphore (GraphicsSem);
		while ((pSolarSysState->MenuState.Initialized > 1
				|| (GLOBAL (CurrentActivity)
				& (START_ENCOUNTER | END_INTERPLANETARY
				| CHECK_ABORT | CHECK_LOAD))
				|| GLOBAL_SIS (CrewEnlisted) == (COUNT)~0)
				&& !Task_ReadState (task, TASK_EXIT))
		{
			InputState = 0;
			ClearSemaphore (GraphicsSem);
			TaskSwitch ();
			SetSemaphore (GraphicsSem);
			NextTime = GetTimeCounter ();
		}

		if (Task_ReadState (task, TASK_EXIT))
		{
			ClearSemaphore (GraphicsSem);
			break;
		}

		OldContext = SetContext (StatusContext);
		if (pSolarSysState->MenuState.CurState
				|| pSolarSysState->MenuState.Initialized == 0)
		{
			InputState = 0;
			if (draw_sys_flags & DRAW_REFRESH)
				goto TheMess;
		}
		else
		{
			if (MenuTransition)
			{
				if (MenuTransition < DEBOUNCE_DELAY
						&& !(InputState & (DEVICE_BUTTON1 | DEVICE_BUTTON2)))
					MenuTransition = 0;
				else
				{
					--MenuTransition;
					InputState &= ~(DEVICE_BUTTON1 | DEVICE_BUTTON2);
				}
			}

TheMess:
			// this is a mess:
			// we have to treat things slightly differently depending on the
			// situation (note that DRAW_REFRESH means we had gone to the
			// menu)
			InnerSystem = (BOOLEAN) (pSolarSysState->pBaseDesc !=
					pSolarSysState->PlanetDesc);
			if (InnerSystem)
			{
				BatchGraphics ();
				if (draw_sys_flags & DRAW_REFRESH)
				{
					InnerSystem = FALSE;
					DrawSystem (pSolarSysState->pBaseDesc->pPrevDesc->radius, TRUE);
				}
			}
			else if (draw_sys_flags & DRAW_REFRESH)
			{
				BatchGraphics ();
				DrawSystem (pSolarSysState->SunDesc[0].radius, FALSE);
			}
			
			if (MenuTransition < DEBOUNCE_DELAY)
				UndrawShip (InputState);
			if (pSolarSysState->MenuState.Initialized != 1)
				InputState = 0;
		}
		
		if (old_radius)
			ScaleSystem ();

		BatchGraphics ();
		
		if (!(draw_sys_flags & DRAW_REFRESH)) // don't repair from Extra or draw ship if forcing repair
		{
			CONTEXT OldContext;
	
			OldContext = SetContext (SpaceContext);
			GetContextClipRect (&r);
			DrawFromExtraScreen (&r);
			SetContext (OldContext);

			// Don't redraw if entering/exiting inner system
			// this screws up ScreenTransition by leaving an image of the
			// ship in the ExtraScreen (which we use for repair)
			if (pSolarSysState->MenuState.CurState == 0
					&& (InnerSystem ^ (BOOLEAN)(pSolarSysState->pBaseDesc != pSolarSysState->PlanetDesc)))
				;
			else
				RedrawQueue (FALSE);
		}
		
		if (InnerSystem)
		{
			if (pSolarSysState->pBaseDesc == pSolarSysState->PlanetDesc)
			{
				// transition screen if we left inner system (if going
				// from outer to inner, ScreenTransition happens elsewhere)
				r.corner.x = SIS_ORG_X;
				r.corner.y = SIS_ORG_Y;
				r.extent.width = SIS_SCREEN_WIDTH;
				r.extent.height = SIS_SCREEN_HEIGHT;
				ScreenTransition (3, &r);
			}
			UnbatchGraphics ();
		}
		else if (draw_sys_flags & DRAW_REFRESH)
		{
			// must set rect for LoadInto... below
			r.corner.x = SIS_ORG_X;
			r.corner.y = SIS_ORG_Y;
			r.extent.width = SIS_SCREEN_WIDTH;
			r.extent.height = SIS_SCREEN_HEIGHT;
			ScreenTransition (3, &r);
			UnbatchGraphics ();
		}
		
		UnbatchGraphics ();
		
		if (draw_sys_flags & UNBATCH_SYS)
		{
			// means we're forcing a redraw/transition from Init- & ChangeSolarSys
			draw_sys_flags &= ~UNBATCH_SYS;
			UnbatchGraphics ();
		}
		
		// LoadInto Extra if we left inner system, or we forced a redraw
		if ((InnerSystem && pSolarSysState->pBaseDesc == pSolarSysState->PlanetDesc)
				|| (draw_sys_flags & DRAW_REFRESH))
		{
			LoadIntoExtraScreen (&r);
			draw_sys_flags &= ~DRAW_REFRESH;
		}

		SetContext (OldContext);
		ClearSemaphore (GraphicsSem);

		if (Task_ReadState (task, TASK_EXIT))
		{
			break;
		}

		if (!(InputState & DEVICE_BUTTON2))
		{
			SleepThreadUntil (NextTime + IP_FRAME_RATE);
			NextTime = GetTimeCounter ();
			if (pSolarSysState->MenuState.CurState
					|| pSolarSysState->MenuState.Initialized != 1)
				InputState = 0;
			else
				InputState = GetInputState (NormalInput);
			JournalInput (InputState);
			
			if (InputState & DEVICE_EXIT)
				InputState = ConfirmExit ();
		}
		else
		{
			MenuTransition = DEBOUNCE_DELAY;
			SuspendGameClock ();

			SetSemaphore (GraphicsSem);
			DrawStatusMessage (NULL_PTR);
			if (LastActivity == CHECK_LOAD)
				pSolarSysState->MenuState.CurState = (ROSTER + 1) + 1;
			else
			{
				ClearSemaphore (GraphicsSem);
				DrawMenuStateStrings (PM_STARMAP, STARMAP);
				SetSemaphore (GraphicsSem);
				pSolarSysState->MenuState.CurState = STARMAP + 1;
			}
			SetFlashRect ((PRECT)~0L, (FRAME)0);
			FlushInput ();
			ClearSemaphore (GraphicsSem);
		}
	}
	FinishTask (task);
	return(0);
}

static void
DrawInnerSystem (void)
{
	SetSemaphore (GraphicsSem);
	DrawSISTitle (GLOBAL_SIS (PlanetName));
	DrawSystem (pSolarSysState->pBaseDesc->pPrevDesc->radius, TRUE);
	ClearSemaphore (GraphicsSem);
}

static BOOLEAN
ValidateOrbits (void)
{
	BYTE i;
	BOOLEAN InnerSystem;
	POINT old_pts[2];
	PPLANET_DESC pCurDesc;

	InnerSystem = (BOOLEAN)(
			pSolarSysState->pBaseDesc == pSolarSysState->MoonDesc
			);
	if (InnerSystem)
	{
		old_pts[0] = GLOBAL (ShipStamp.origin);
		old_pts[1] = GLOBAL (ip_location);
		GLOBAL (ip_location) =
				pSolarSysState->SunDesc[0].location;
	}

	pSolarSysState->SunDesc[0].radius = MAX_ZOOM_RADIUS << 1;
	FindRadius ();

	pSolarSysState->pBaseDesc = 0;
	for (i = pSolarSysState->SunDesc[0].NumPlanets,
			pCurDesc = &pSolarSysState->PlanetDesc[0];
			i; --i, ++pCurDesc)
	{
		DrawOrbit (pCurDesc,
				DISPLAY_FACTOR,
				DISPLAY_FACTOR >> 2,
				DISPLAY_FACTOR >> 1,
				pSolarSysState->SunDesc[0].radius);
	}

	if (!InnerSystem)
		pSolarSysState->pBaseDesc = pSolarSysState->PlanetDesc;
	else
	{
		pSolarSysState->pBaseDesc = pSolarSysState->MoonDesc;
		GLOBAL (ShipStamp.origin) = old_pts[0];
		GLOBAL (ip_location) = old_pts[1];
	}

	return (InnerSystem);
}

void
ChangeSolarSys (void)
{
	if (pSolarSysState->MenuState.Initialized == 0)
	{
StartGroups:
		++pSolarSysState->MenuState.Initialized;
		if (pSolarSysState->MenuState.flash_task == 0)
		{
			DrawMenuStateStrings (PM_STARMAP, -(PM_NAVIGATE - PM_SCAN));

			SetSemaphore (GraphicsSem);
			RepairSISBorder ();

			InitDisplayList ();
			DoMissions ();

			// if entering new system (NOT from load),
			// force redraw and transition in IPtask_func
			if ((draw_sys_flags & UNBATCH_SYS)
					&& LastActivity != (CHECK_LOAD | CHECK_RESTART))
				draw_sys_flags |= DRAW_REFRESH;
				
			CheckIntersect (TRUE);
			
			pSolarSysState->MenuState.flash_task =
					AssignTask (IPtask_func, 6144,
					"flash solar system menu");
			ClearSemaphore (GraphicsSem);

			if (!PLRPlaying ((MUSIC_REF)~0) && LastActivity != CHECK_LOAD)
			{
				PlayMusic (SpaceMusic, TRUE, 1);
				if (LastActivity == (CHECK_LOAD | CHECK_RESTART))
				{
					BYTE clut_buf[] = {FadeAllToColor};

					LastActivity = 0;
					if (draw_sys_flags & UNBATCH_SYS)
					{
						draw_sys_flags &= ~UNBATCH_SYS;
						UnbatchGraphics ();
					}
					SetSemaphore (GraphicsSem);
					while (pSolarSysState->SunDesc[0].radius == (MAX_ZOOM_RADIUS << 1))
					{
						ClearSemaphore (GraphicsSem);
						TaskSwitch ();
						SetSemaphore (GraphicsSem);
					}
					ClearSemaphore (GraphicsSem);
					XFormColorMap ((COLORMAPPTR)clut_buf, ONE_SECOND / 2);
				}
			}

			ResumeGameClock ();
			SetGameClockRate (INTERPLANETARY_CLOCK_RATE);
		}
	}
	else
	{
		if (pSolarSysState->MenuState.flash_task)
		{
			SuspendGameClock ();
			FreeSolarSys ();

			if (pSolarSysState->pOrbitalDesc->pPrevDesc !=
					&pSolarSysState->SunDesc[0])
				GLOBAL (ShipStamp.origin) =
						pSolarSysState->pOrbitalDesc->image.origin;
			else
			{
				GLOBAL (ShipStamp.origin.x) =
						SIS_SCREEN_WIDTH >> 1;
				GLOBAL (ShipStamp.origin.y) =
						SIS_SCREEN_HEIGHT >> 1;
			}
  
		}

		GetPlanetInfo ();
		(*pSolarSysState->GenFunc) (GENERATE_ORBITAL);
		LastActivity &= ~(CHECK_LOAD | CHECK_RESTART);
		if ((GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD | START_ENCOUNTER))
				|| GLOBAL_SIS (CrewEnlisted) == (COUNT)~0
				|| GET_GAME_STATE (CHMMR_BOMB_STATE) == 2)
			return;

		if (pSolarSysState->MenuState.flash_task == 0)
		{
			LoadSolarSys ();
			ValidateOrbits ();
			DrawInnerSystem ();

			goto StartGroups;
		}
		else
		{
			DrawMenuStateStrings (PM_SCAN, SCAN);
			SetSemaphore (GraphicsSem);
			pSolarSysState->MenuState.CurState = SCAN + 1;
			SetFlashRect ((PRECT)~0L, (FRAME)0);
			FlushInput ();
			ClearSemaphore (GraphicsSem);
		}
	}
}

static void
InitSolarSys (void)
{
	BOOLEAN InnerSystem;
	BOOLEAN Reentry;

	SetSemaphore (GraphicsSem);

LoadIPData ();
LoadLanderData ();
	ClearSemaphore (GraphicsSem);

	pSolarSysState->MenuState.InputFunc = DoFlagshipCommands;

	Reentry = (BOOLEAN)(GLOBAL (ShipStamp.frame) != 0);
	if (!Reentry)
	{
		GLOBAL (autopilot.x) =
				GLOBAL (autopilot.y) = ~0;

		GLOBAL (ShipStamp.origin.x) = SIS_SCREEN_WIDTH >> 1;
		GLOBAL (ShipStamp.origin.y) = SIS_SCREEN_HEIGHT >> 1;
		GLOBAL (ShipStamp.origin.y) += (SIS_SCREEN_HEIGHT >> 1) - 1;

		pSolarSysState->SunDesc[0].radius = MAX_ZOOM_RADIUS;
		XFormIPLoc (&GLOBAL (ShipStamp.origin),
				&GLOBAL (ip_location),
				FALSE);
	}

	LoadSolarSys ();
	InnerSystem = ValidateOrbits ();

	if (Reentry)
	{
		(*pSolarSysState->GenFunc) (REINIT_NPCS);
	}
	else
	{
		battle_counter = 0;
		GLOBAL (BattleGroupRef) = 0;
		ReinitQueue (&GLOBAL (npc_built_ship_q));
		(*pSolarSysState->GenFunc) (INIT_NPCS);
	}

	if (pSolarSysState->MenuState.Initialized == 0)
	{
		BatchGraphics ();
		draw_sys_flags |= UNBATCH_SYS;
		
		if (LastActivity & (CHECK_LOAD | CHECK_RESTART))
		{
			if ((LastActivity & (CHECK_LOAD | CHECK_RESTART)) == LastActivity)
			{
				extern ACTIVITY NextActivity;

				DrawSISFrame ();
				if (NextActivity)
					LastActivity &= ~(CHECK_LOAD | CHECK_RESTART);
			}
			else
			{
				SetSemaphore (GraphicsSem);
				ClearSISRect (DRAW_SIS_DISPLAY);
				ClearSemaphore (GraphicsSem);

				LastActivity &= ~CHECK_LOAD;
			}
		}

		// Enabled SetSemaphore and ClearSemaphore again, as in 3DO code originally.
		// This should fix the 'entering star' lockup/messed graphics problems.
		// 2002/11/30 by Mika
		SetSemaphore (GraphicsSem);
		DrawSISMessage (NULL_PTR);
		SetContext (SpaceContext);
		SetContextFGFrame (Screen);
		SetContextBGFrame ((FRAME)0);
		SetContextDrawState (DEST_PIXMAP | DRAW_REPLACE);
		SetContextBackGroundColor (BLACK_COLOR);
		ClearSemaphore (GraphicsSem);

		if (InnerSystem)
		{
			SetGraphicGrabOther (1); // since Unbatch won't have flipped yet
			DrawInnerSystem ();
			SetGraphicGrabOther (0);
			if (draw_sys_flags & UNBATCH_SYS)
			{
				draw_sys_flags &= ~UNBATCH_SYS;
				ScreenTransition (3, 0);
				UnbatchGraphics ();
				LoadIntoExtraScreen (0);
			}
		}
		else
		{
			SetSemaphore (GraphicsSem);
			DrawHyperCoords (CurStarDescPtr->star_pt); /* Adjust position */
			ClearSemaphore (GraphicsSem);

					/* force a redraw */
			pSolarSysState->SunDesc[0].radius = MAX_ZOOM_RADIUS << 1;
		}
	}
}

static void
UninitSolarSys (void)
{
	FreeSolarSys ();

//FreeLanderData ();
//FreeIPData ();

	if (GLOBAL (CurrentActivity) & END_INTERPLANETARY)
	{
		GLOBAL (CurrentActivity) &= ~END_INTERPLANETARY;
		(*pSolarSysState->GenFunc) (UNINIT_NPCS);

		SET_GAME_STATE (USED_BROADCASTER, 0);
	}
	else if ((GLOBAL (CurrentActivity) & START_ENCOUNTER) && battle_counter)
	{
		GetGroupInfo (GLOBAL (BattleGroupRef), (BYTE)battle_counter);
		if (HIWORD (GLOBAL (ShipStamp.frame)) == 0)
		{
			BYTE i;
			DWORD best_dist;
			PPLANET_DESC pCurDesc;

			best_dist = ~0L;
			for (i = 0, pCurDesc = pSolarSysState->PlanetDesc;
					i < pSolarSysState->SunDesc[0].NumPlanets; ++i, ++pCurDesc)
			{
				SIZE dx, dy;
				DWORD dist;

				dx = GLOBAL (ShipStamp.origin.x)
						- pCurDesc->image.origin.x;
				dy = GLOBAL (ShipStamp.origin.y)
						- pCurDesc->image.origin.y;
				if ((dist = (DWORD)((long)dx * dx + (long)dy * dy)) < best_dist)
				{
					best_dist = dist;
					pSolarSysState->pBaseDesc = pCurDesc;
				}
			}

			(*pSolarSysState->GenFunc) (GENERATE_NAME);
		}
	}
}

void
GenerateRandomIP (BYTE control)
{
	COUNT i;
	DWORD rand_val;
	PPLANET_DESC pCurDesc;

	switch (control)
	{
		case INIT_NPCS:
			if (!GetGroupInfo (GLOBAL (BattleGroupRef), (BYTE)~0))
			{
				GLOBAL (BattleGroupRef) = 0;
				BuildGroups ();
			}
			break;
		case REINIT_NPCS:
			GetGroupInfo (0L, 0);
			break;
		case UNINIT_NPCS:
			PutGroupInfo (0L, (BYTE)~0);
			ReinitQueue (&GLOBAL (npc_built_ship_q));
			break;
		case GENERATE_MINERAL:
			GenerateMineralDeposits (&pSolarSysState->SysInfo, &pSolarSysState->CurNode);
			break;
		case GENERATE_ENERGY:
			pSolarSysState->CurNode = 0;
			break;
		case GENERATE_LIFE:
			GenerateLifeForms (&pSolarSysState->SysInfo, &pSolarSysState->CurNode);
			break;
		case GENERATE_ORBITAL:
		{
#ifdef DEBUG
			if (pSolarSysState->pOrbitalDesc->pPrevDesc ==
					pSolarSysState->SunDesc)
				fprintf (stderr, "Planet index = %d\n", pSolarSysState->pOrbitalDesc - pSolarSysState->PlanetDesc);
			else
				fprintf (stderr, "Planet index = %d, Moon index = %d\n",
						pSolarSysState->pOrbitalDesc->pPrevDesc - pSolarSysState->PlanetDesc,
						pSolarSysState->pOrbitalDesc - pSolarSysState->MoonDesc);
#endif /* DEBUG */
			rand_val = DoPlanetaryAnalysis (
					&pSolarSysState->SysInfo, pSolarSysState->pOrbitalDesc
					);

			pSolarSysState->SysInfo.PlanetInfo.ScanSeed[BIOLOGICAL_SCAN] = rand_val;
			i = (COUNT)~0;
			rand_val = GenerateLifeForms (&pSolarSysState->SysInfo, &i);

			pSolarSysState->SysInfo.PlanetInfo.ScanSeed[MINERAL_SCAN] = rand_val;
			i = (COUNT)~0;
			GenerateMineralDeposits (&pSolarSysState->SysInfo, &i);

			pSolarSysState->SysInfo.PlanetInfo.ScanSeed[ENERGY_SCAN] = rand_val;
			LoadPlanet (FALSE);
			break;
		}
		case GENERATE_NAME:
		{
			i = pSolarSysState->pBaseDesc - pSolarSysState->PlanetDesc;
			wsprintf (GLOBAL_SIS (PlanetName), "%s", GAME_STRING (PLANET_NUMBER_BASE + (9 + 7) + i));
			SET_GAME_STATE (BATTLE_PLANET, pSolarSysState->PlanetDesc[i].data_index);
			break;
		}
		case GENERATE_MOONS:
			FillOrbits (
					pSolarSysState->pBaseDesc->NumPlanets,
					&pSolarSysState->MoonDesc[0], FALSE
					);
			break;
		case GENERATE_PLANETS:
		{
			FillOrbits ((BYTE)~0, &pSolarSysState->PlanetDesc[0], FALSE);
			for (i = pSolarSysState->SunDesc[0].NumPlanets,
					pCurDesc = &pSolarSysState->PlanetDesc[0]; i; --i, ++pCurDesc)
			{
				BYTE byte_val;
				BYTE num_moons;

				rand_val = TFB_Random ();
				byte_val = LOBYTE (rand_val);

				num_moons = 0;
				switch (PLANSIZE (PlanData[
						pCurDesc->data_index & ~PLANET_SHIELDED
						].Type))
				{
					case LARGE_ROCKY_WORLD:
						if (byte_val < 0x00FF * 25 / 100)
						{
							if (byte_val < 0x00FF * 5 / 100)
								++num_moons;
							++num_moons;
						}
						break;
					case GAS_GIANT:
						if (byte_val < 0x00FF * 90 / 100)
						{
							if (byte_val < 0x00FF * 75 / 100)
							{
								if (byte_val < 0x00FF * 50 / 100)
								{
									if (byte_val < 0x00FF * 25 / 100)
										++num_moons;
									++num_moons;
								}
								++num_moons;
							}
							++num_moons;
						}
						break;
				}
				pCurDesc->NumPlanets = num_moons;
			}
			break;
		}
	}
}

void
DrawSystem (SIZE radius, BOOLEAN IsInnerSystem)
{
	BYTE i;
	PPLANET_DESC pCurDesc, pBaseDesc;

	BatchGraphics ();
	if (draw_sys_flags & DRAW_STARS)
		DrawStarBackGround (FALSE);

	if (!IsInnerSystem)
	{
		pBaseDesc = pSolarSysState->PlanetDesc;
		pSolarSysState->pOrbitalDesc = 0;
	}
	else
	{
		pBaseDesc = pSolarSysState->pBaseDesc;

		pCurDesc = pBaseDesc->pPrevDesc;
		pSolarSysState->pOrbitalDesc = pCurDesc;
		DrawOrbit (pCurDesc,
				DISPLAY_FACTOR << 2,
				DISPLAY_FACTOR,
				DISPLAY_FACTOR << 1,
				radius);
	}

	for (i = pBaseDesc->pPrevDesc->NumPlanets,
			pCurDesc = pBaseDesc; i; --i, ++pCurDesc)
	{
		if (IsInnerSystem)
			DrawOrbit (pCurDesc, 2, 1, 1, 2);
		else
			DrawOrbit (pCurDesc,
					DISPLAY_FACTOR,
					DISPLAY_FACTOR >> 2,
					DISPLAY_FACTOR >> 1,
					radius);
	}

	if (IsInnerSystem)
		DrawSISTitle (GLOBAL_SIS (PlanetName));
	else
	{
		SIZE index;

		index = 0;
		if (radius <= (MAX_ZOOM_RADIUS >> 1))
		{
			++index;
			if (radius <= (MAX_ZOOM_RADIUS >> 2))
				++index;
		}

		pCurDesc = &pSolarSysState->SunDesc[0];
		pCurDesc->image.origin.x = SIS_SCREEN_WIDTH >> 1;
		pCurDesc->image.origin.y = SIS_SCREEN_HEIGHT >> 1;
		pCurDesc->image.frame =
				SetRelFrameIndex (SunFrame, index);

		index = pSolarSysState->FirstPlanetIndex;
	if (draw_sys_flags & DRAW_PLANETS)
	{
		for (;;)
		{
			COUNT color_index;

			pCurDesc = &pSolarSysState->PlanetDesc[index];
			/* Star color fix - draw the star using OrbitalCMap */
			if (pCurDesc == &pSolarSysState->SunDesc[0])
				color_index = STAR_COLOR (CurStarDescPtr->Type);
			else
				color_index = PLANCOLOR (PlanData[
						pCurDesc->data_index & ~PLANET_SHIELDED
						].Type);

			SetColorMap (GetColorMapAddress (
					SetAbsColorMapIndex (OrbitalCMap, color_index)
					));
			DrawStamp (&pCurDesc->image);

			if (index == pSolarSysState->LastPlanetIndex)
				break;
			index = pCurDesc->NextIndex;
		}
	}

		if (pSolarSysState->pBaseDesc == pSolarSysState->PlanetDesc)
			XFormIPLoc (&GLOBAL (ip_location),
					&GLOBAL (ShipStamp.origin),
					TRUE);
		else
			XFormIPLoc (&pSolarSysState->SunDesc[0].location,
					&GLOBAL (ShipStamp.origin),
					TRUE);

		if (draw_sys_flags & DRAW_HYPER_COORDS)
			DrawHyperCoords (CurStarDescPtr->star_pt);
	}

	UnbatchGraphics ();

	SetContext (SpaceContext);
	
	if (draw_sys_flags & GRAB_BKGND)
	{
		RECT r;

		GetContextClipRect (&r);
		LoadIntoExtraScreen (&r);
	}

//    pSolarSysState->WaitIntersect = TRUE;
}

void
DrawStarBackGround (BOOLEAN ForPlanet)
{
	COUNT i, j;
	DWORD rand_val;
	STAMP s;
	DWORD old_seed;

	SetContext (SpaceContext);
	SetContextBGFrame ((FRAME)0);
	SetContextDrawState (DEST_PIXMAP | DRAW_REPLACE);
	SetContextBackGroundColor (BLACK_COLOR);

	ClearDrawable ();

	old_seed = TFB_SeedRandom (
			MAKE_DWORD (CurStarDescPtr->star_pt.x,
			CurStarDescPtr->star_pt.y)
			);

#define NUM_DIM_PIECES 8
	s.frame = SpaceJunkFrame;
	for (i = 0; i < NUM_DIM_PIECES; ++i)
	{
#define NUM_DIM_DRAWN 5
		for (j = 0; j < NUM_DIM_DRAWN; ++j)
		{
			rand_val = TFB_Random ();
			s.origin.x = LOWORD (rand_val) % SIS_SCREEN_WIDTH;
			s.origin.y = HIWORD (rand_val) % SIS_SCREEN_HEIGHT;

			DrawStamp (&s);
		}
		s.frame = IncFrameIndex (s.frame);
	}
#define NUM_BRT_PIECES 8
	for (i = 0; i < NUM_BRT_PIECES; ++i)
	{
#define NUM_BRT_DRAWN 30
		for (j = 0; j < NUM_BRT_DRAWN; ++j)
		{
			rand_val = TFB_Random ();
			s.origin.x = LOWORD (rand_val) % SIS_SCREEN_WIDTH;
			s.origin.y = HIWORD (rand_val) % SIS_SCREEN_HEIGHT;

			DrawStamp (&s);
		}
		s.frame = IncFrameIndex (s.frame);
	}

	if (ForPlanet)
	{
		RECT r;

		// 2002-12-13 PhracturedBlue's fix to planet changing color when using device problem
		/*if (pSolarSysState->MenuState.flash_task
				|| (LastActivity & CHECK_LOAD))
			RenderTopography (TRUE);*/

		BatchGraphics ();

		SetContext (ScreenContext);

		SetContextForeGroundColor (BUILD_COLOR (MAKE_RGB15 (0xA, 0xA, 0xA), 0x08));
		r.corner.x = SIS_ORG_X - 1;
		r.corner.y = (SIS_ORG_Y + SIS_SCREEN_HEIGHT) - MAP_HEIGHT - 4;
		r.extent.width = SIS_SCREEN_WIDTH + 2;
		r.extent.height = 3;
		DrawFilledRectangle (&r);

		SetContextForeGroundColor (BUILD_COLOR (MAKE_RGB15 (0x8, 0x8, 0x8), 0x1F));
		r.extent.width = 1;
		r.extent.height = MAP_HEIGHT + 1;
		r.corner.y = (SIS_ORG_Y + SIS_SCREEN_HEIGHT) - MAP_HEIGHT;
		r.corner.x = (SIS_ORG_X + SIS_SCREEN_WIDTH) - MAP_WIDTH - 1;
		DrawFilledRectangle (&r);
		r.corner.x = SIS_ORG_X + SIS_SCREEN_WIDTH;
		DrawFilledRectangle (&r);
		r.extent.width = SIS_SCREEN_WIDTH + 1;
		r.extent.height = 1;
		r.corner.x = SIS_ORG_X;
		r.corner.y = (SIS_ORG_Y + SIS_SCREEN_HEIGHT) - MAP_HEIGHT - 5;
		DrawFilledRectangle (&r);

		SetContextForeGroundColor (BUILD_COLOR (MAKE_RGB15 (0x10, 0x10, 0x10), 0x19));
		r.corner.y = (SIS_ORG_Y + SIS_SCREEN_HEIGHT) - MAP_HEIGHT - 1;
		r.extent.width = MAP_WIDTH + 2;
		r.corner.x = (SIS_ORG_X + SIS_SCREEN_WIDTH) - MAP_WIDTH - 1;
		DrawFilledRectangle (&r);
		r.extent.width = 1;
		r.extent.height = MAP_HEIGHT + 1;
		r.corner.y = (SIS_ORG_Y + SIS_SCREEN_HEIGHT) - MAP_HEIGHT;
		r.corner.x = (SIS_ORG_X + SIS_SCREEN_WIDTH) - MAP_WIDTH - 1;
		DrawFilledRectangle (&r);
		
		UnbatchGraphics ();
	}

	TFB_SeedRandom (old_seed);
}

void
XFormIPLoc (PPOINT pIn, PPOINT pOut, BOOLEAN ToDisplay)
{
	if (ToDisplay)
	{
		pOut->x = (SIS_SCREEN_WIDTH >> 1)
				+ (SIZE)((long)pIn->x
				* (DISPLAY_FACTOR >> 1)
// / (long)pSolarSysState->SunDesc[0].radius);
				/ pSolarSysState->SunDesc[0].radius);
		pOut->y = (SIS_SCREEN_HEIGHT >> 1)
				+ (SIZE)((long)pIn->y
				* (DISPLAY_FACTOR >> 1)
// / (long)pSolarSysState->SunDesc[0].radius);
				/ pSolarSysState->SunDesc[0].radius);
	}
	else
	{
		pOut->x = (SIZE)((long)(pIn->x
				- (SIS_SCREEN_WIDTH >> 1))
// * (long)pSolarSysState->SunDesc[0].radius
				* pSolarSysState->SunDesc[0].radius
				/ (DISPLAY_FACTOR >> 1));
		pOut->y = (SIZE)((long)(pIn->y
				- (SIS_SCREEN_HEIGHT >> 1))
// * (long)pSolarSysState->SunDesc[0].radius
				* pSolarSysState->SunDesc[0].radius
				/ (DISPLAY_FACTOR >> 1));
	}
}

void
ExploreSolarSys (void)
{
	SOLARSYS_STATE SolarSysState;

	if (CurStarDescPtr == 0)
	{
		POINT universe;

		universe.x = LOGX_TO_UNIVERSE (GLOBAL_SIS (log_x));
		universe.y = LOGY_TO_UNIVERSE (GLOBAL_SIS (log_y));
		if ((CurStarDescPtr = FindStar (0, &universe, 1, 1)) == 0)
			;
	}
	GLOBAL_SIS (log_x) = UNIVERSE_TO_LOGX (CurStarDescPtr->star_pt.x);
	GLOBAL_SIS (log_y) = UNIVERSE_TO_LOGY (CurStarDescPtr->star_pt.y);

	pSolarSysState = &SolarSysState;

	memset (pSolarSysState, 0, sizeof (*pSolarSysState));

	SolarSysState.GenFunc = GenerateIP (CurStarDescPtr->Index);

	InitSolarSys ();
	DoInput ((PVOID)&SolarSysState.MenuState);
	UninitSolarSys ();
	pSolarSysState = 0;
}

