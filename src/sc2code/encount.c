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

#include "encount.h"

#include "battle.h"
#include "build.h"
#include "colors.h"
#include "controls.h"
#include "gameopt.h"
#include "gamestr.h"
#include "globdata.h"
#include "init.h"
#include "intel.h"
#include "nameref.h"
#include "resinst.h"
#include "settings.h"
#include "setup.h"
#include "sounds.h"
#include "libs/graphics/gfx_common.h"
#include "libs/mathlib.h"
#include "libs/inplib.h"


static void DrawFadeText (const UNICODE *str1, const UNICODE *str2,
		BOOLEAN fade_in, PRECT pRect);


static BOOLEAN
DoSelectAction (PMENU_STATE pMS)
{
	SetMenuSounds (MENU_SOUND_ARROWS, MENU_SOUND_SELECT);
	if (GLOBAL (CurrentActivity) & CHECK_ABORT)
	{
		pMS->CurState = ATTACK + 1;
		return (FALSE);
	}
	if (!pMS->Initialized)
	{
		pMS->Initialized = TRUE;
		pMS->InputFunc = DoSelectAction;
	}
	else if (PulsedInputState.key[KEY_MENU_SELECT])
	{
		switch (pMS->CurState)
		{
			case HAIL:
			case ATTACK:
				if (LOBYTE (GLOBAL (CurrentActivity)) == IN_LAST_BATTLE)
					pMS->CurState = HAIL;
				return (FALSE);
			case ATTACK + 1:
				pMS->Initialized = GameOptions ();
				if (pMS->Initialized)
				{
					DrawMenuStateStrings (PM_CONVERSE, pMS->CurState);
					LockMutex (GraphicsLock);
					SetFlashRect ((PRECT)~0L, (FRAME)0);
					UnlockMutex (GraphicsLock);
				}
				return ((BOOLEAN)pMS->Initialized);
			default:
				printf ("Unknown option: %d\n", pMS->CurState);
		}
	}
	DoMenuChooser (pMS, PM_CONVERSE);
	return (TRUE);
}

void
BuildBattle (COUNT which_player)
{
	PQUEUE pQueue;
	HSTARSHIP hStarShip, hNextShip;
	HSTARSHIP hBuiltShip;
	STARSHIPPTR BuiltShipPtr;

	battle_counter = 0;
	if (GetHeadLink (&GLOBAL (npc_built_ship_q)) == 0)
	{
		SET_GAME_STATE (BATTLE_SEGUE, 0);
		return;
	}

	if (which_player == 0)
		pQueue = &GLOBAL (built_ship_q);
	else
	{
		switch (LOBYTE (GLOBAL (CurrentActivity)))
		{
			case IN_LAST_BATTLE:
				load_gravity_well (NUMBER_OF_PLANET_TYPES);
				break;
			case IN_HYPERSPACE:
				load_gravity_well ((BYTE)((COUNT)TFB_Random ()
						% NUMBER_OF_PLANET_TYPES));
				break;
			default:
				SET_GAME_STATE (ESCAPE_COUNTER, 110);
				load_gravity_well (GET_GAME_STATE (BATTLE_PLANET));
				break;
		}
		pQueue = &GLOBAL (npc_built_ship_q);
	}

	ReinitQueue (&race_q[which_player]);
	for (hStarShip = GetHeadLink (pQueue);
			hStarShip != 0; hStarShip = hNextShip)
	{
		SHIP_FRAGMENTPTR FragPtr;

		FragPtr = (SHIP_FRAGMENTPTR)LockStarShip (pQueue, hStarShip);
		hNextShip = _GetSuccLink (FragPtr);

		hBuiltShip = Build (&race_q[which_player],
				GET_RACE_ID (FragPtr) == SAMATRA_SHIP ?
				SAMATRA_RES_INDEX : FragPtr->RaceResIndex,
				1 << which_player,
				StarShipCaptain (FragPtr));
		if (hBuiltShip)
		{
			BuiltShipPtr = LockStarShip (
					&race_q[which_player],
					hBuiltShip
					);
			BuiltShipPtr->captains_name_index =
					StarShipCaptain (BuiltShipPtr);
			BuiltShipPtr->cur_status_flags =
					StarShipPlayer (BuiltShipPtr);
			if (FragPtr->ShipInfo.crew_level != (BYTE)~0)
				BuiltShipPtr->special_counter = FragPtr->ShipInfo.crew_level;
			else /* if infinite ships */
				BuiltShipPtr->special_counter = FragPtr->ShipInfo.max_crew;
			BuiltShipPtr->RaceDescPtr =
					(RACE_DESCPTR)&FragPtr->ShipInfo;

			UnlockStarShip (
					&race_q[which_player],
					hBuiltShip
					);
		}

		UnlockStarShip (pQueue, hStarShip);
	}

	if (which_player == 0
			&& (hBuiltShip = Build (&race_q[0],
			SIS_RES_INDEX, GOOD_GUY, 0)))
	{
		BuiltShipPtr = LockStarShip (
				&race_q[0],
				hBuiltShip
				);
		BuiltShipPtr->captains_name_index =
				StarShipCaptain (BuiltShipPtr);
		BuiltShipPtr->cur_status_flags =
				StarShipPlayer (BuiltShipPtr);
		if (GLOBAL_SIS (CrewEnlisted) >= MAX_CREW_SIZE - 1)
			BuiltShipPtr->special_counter = MAX_CREW_SIZE;
		else
			BuiltShipPtr->special_counter = (BYTE)(
					GLOBAL_SIS (CrewEnlisted) + 1
					);
		BuiltShipPtr->energy_counter = MAX_ENERGY_SIZE;
		UnlockStarShip (
				&race_q[0],
				hBuiltShip
				);
	}
}

/*
 * Encountering an alien.
 * Draws the encounter screen, plays the red alert music, and
 * waits for a decision of the player on how to handle the situation.
 * Returns either HAIL or ATTACK.
 */
COUNT
InitEncounter (void)
{
	COUNT i;
	MEM_HANDLE hOldIndex;
	FRAME SegueFrame;
	STAMP s;
	TEXT t;
	extern FRAME planet[];
	MUSIC_REF MR;

	LockMutex (GraphicsLock);

	hOldIndex = SetResourceIndex (hResIndex);

	SetContext (SpaceContext);
	SetContextFont (TinyFont);

	MR = LoadMusicInstance (REDALERT_MUSIC);
	PlayMusic (MR, FALSE, 1);
	SegueFrame = CaptureDrawable (LoadGraphic (SEGUE_PMAP_ANIM));
	UnlockMutex (GraphicsLock);
	while (PLRPlaying (MR))
		TaskSwitch ();
	StopMusic ();
	DestroyMusic (MR);
	LockMutex (GraphicsLock);
	s.origin.x = s.origin.y = 0;

	SetTransitionSource (NULL);
	BatchGraphics ();
	
	SetContextBackGroundColor (BLACK_COLOR);
	ClearDrawable ();
	s.frame = SegueFrame;
	DrawStamp (&s);

//    t.baseline.x = SIS_SCREEN_WIDTH >> 1;
	t.baseline.x = (SIS_SCREEN_WIDTH >> 1) + 1;
	t.baseline.y = 10;
	t.align = ALIGN_CENTER;

	SetContextFont (MicroFont);
	SetContextForeGroundColor (BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0x14), 0x01));
	if (LOBYTE (GLOBAL (CurrentActivity)) == IN_HYPERSPACE)
	{
		t.pStr = GAME_STRING (ENCOUNTER_STRING_BASE + 0);
		t.CharCount = (COUNT)~0;
		font_DrawText (&t);
		t.baseline.y += 12;
		t.pStr = GAME_STRING (ENCOUNTER_STRING_BASE + 1);
		t.CharCount = (COUNT)~0;
		font_DrawText (&t);
	}
	else
	{
		UNICODE buf[40];

		t.pStr = GAME_STRING (ENCOUNTER_STRING_BASE + 2);
		t.CharCount = (COUNT)~0;
		font_DrawText (&t);
		t.baseline.y += 12;
		GetClusterName (CurStarDescPtr, buf);
		t.pStr = buf;
		t.CharCount = (COUNT)~0;
		font_DrawText (&t);
		t.baseline.y += 12;
		t.pStr = GLOBAL_SIS (PlanetName);
		t.CharCount = (COUNT)~0;
		font_DrawText (&t);
	}

	s.origin.x = SIS_SCREEN_WIDTH >> 1;
	s.origin.y = SIS_SCREEN_HEIGHT >> 1;
	s.frame = planet[0];
	DrawStamp (&s);

	if (LOBYTE (GLOBAL (CurrentActivity)) != IN_LAST_BATTLE)
	{
#define NUM_DISPLAY_PTS (sizeof (display_pt) / sizeof (display_pt[0]))
		HSTARSHIP hStarShip, hNextShip;
		POINT display_pt[] =
		{
			{10, 51},
			{-10, 51},
			{33, 40},
			{-33, 40},
			{49, 18},
			{-49, 18},
			{52, -6},
			{-52, -6},
			{44, -27},
			{-44, -27},
		};

		for (hStarShip = GetHeadLink (&race_q[1]), i = 0;
				hStarShip && i < 60; hStarShip = hNextShip, ++i)
		{
			RECT r;
			STARSHIPPTR StarShipPtr;

			StarShipPtr = LockStarShip (&race_q[1], hStarShip);
			if (StarShipPtr->RaceDescPtr->ship_info.crew_level != (BYTE)~0)
				hNextShip = _GetSuccLink (StarShipPtr);
			else /* if infinite ships */
				hNextShip = hStarShip;

			s.origin = display_pt[i % NUM_DISPLAY_PTS];
			if (i >= NUM_DISPLAY_PTS)
			{
				COUNT angle, radius;

				radius = square_root ((long)s.origin.x * s.origin.x
						+ (long)s.origin.y * s.origin.y)
						+ ((i / NUM_DISPLAY_PTS) * 18);

				angle = ARCTAN (s.origin.x, s.origin.y);
				s.origin.x = COSINE (angle, radius);
				s.origin.y = SINE (angle, radius);
			}
			s.frame = SetAbsFrameIndex (
					StarShipPtr->RaceDescPtr->ship_info.icons, 0
					);
			GetFrameRect (s.frame, &r);
			s.origin.x += (SIS_SCREEN_WIDTH >> 1) - (r.extent.width >> 1);
			s.origin.y += (SIS_SCREEN_HEIGHT >> 1) - (r.extent.height >> 1);
			DrawStamp (&s);

			UnlockStarShip (&race_q[1], hStarShip);
		}
	}

	UnbatchGraphics ();
	DestroyDrawable (ReleaseDrawable (SegueFrame));
	ScreenTransition (3, NULL);

	SetResourceIndex (hOldIndex);

	UnlockMutex (GraphicsLock);

	{
		MENU_STATE MenuState;

		MenuState.InputFunc = DoSelectAction;
		MenuState.Initialized = FALSE;

		DrawMenuStateStrings (PM_CONVERSE, MenuState.CurState = HAIL);
		LockMutex (GraphicsLock);
		SetFlashRect ((PRECT)~0L, (FRAME)0);
		UnlockMutex (GraphicsLock);

		DoInput ((PVOID)&MenuState, TRUE);

		LockMutex (GraphicsLock);
		SetFlashRect (NULL_PTR, (FRAME)0);
		UnlockMutex (GraphicsLock);

		return (MenuState.CurState);
	}
}

static void
DrawFadeText (const UNICODE *str1, const UNICODE *str2, BOOLEAN fade_in,
		PRECT pRect)
{
	SIZE i;
	DWORD TimeIn;
	TEXT t1, t2;
	COLOR fade_cycle[] =
	{
		BUILD_COLOR (MAKE_RGB15 (0xA, 0xA, 0xA), 0x1D),
		BUILD_COLOR (MAKE_RGB15 (0x9, 0x9, 0x9), 0x1E),
		BUILD_COLOR (MAKE_RGB15 (0x8, 0x8, 0x8), 0x1F),
		BUILD_COLOR (MAKE_RGB15 (0x6, 0x6, 0x6), 0x20),
		BUILD_COLOR (MAKE_RGB15 (0x5, 0x5, 0x5), 0x21),
		BUILD_COLOR (MAKE_RGB15 (0x4, 0x4, 0x4), 0x22),
		BUILD_COLOR (MAKE_RGB15 (0x3, 0x3, 0x3), 0x23),
	};
#define NUM_FADES (sizeof (fade_cycle) / sizeof (fade_cycle[0]))

	t1.baseline.x = pRect->corner.x + 100;
	t1.baseline.y = pRect->corner.y + 45;
	t1.align = ALIGN_CENTER;
	t1.pStr = str1;
	t1.CharCount = (COUNT)~0;
	t2 = t1;
	t2.baseline.y += 11;
	t2.pStr = str2;

	FlushInput ();
	TimeIn = GetTimeCounter ();
	if (fade_in)
	{
		for (i = 0; i < (SIZE) NUM_FADES; ++i)
		{
			UnlockMutex (GraphicsLock);
			if (AnyButtonPress (TRUE))
				i = NUM_FADES - 1;
			LockMutex (GraphicsLock);

			SetContextForeGroundColor (fade_cycle[i]);
			font_DrawText (&t1);
			font_DrawText (&t2);
			SleepThreadUntil (TimeIn + (ONE_SECOND / 20));
			TimeIn = GetTimeCounter ();
		}
	}
	else
	{
		for (i = NUM_FADES - 1; i >= 0; --i)
		{
			UnlockMutex (GraphicsLock);
			if (AnyButtonPress (TRUE))
				i = 0;
			LockMutex (GraphicsLock);

			SetContextForeGroundColor (fade_cycle[i]);
			font_DrawText (&t1);
			font_DrawText (&t2);
			SleepThreadUntil (TimeIn + (ONE_SECOND / 20));
			TimeIn = GetTimeCounter ();
		}
		SetContextForeGroundColor (BUILD_COLOR (MAKE_RGB15 (0xA, 0xA, 0xA),
				0x08));
		font_DrawText (&t1);
		font_DrawText (&t2);
	}
}

COUNT
UninitEncounter (void)
{
	COUNT ships_killed;

	ships_killed = 0;

	LockMutex (GraphicsLock);
	free_gravity_well ();

	if ((GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
			|| GLOBAL_SIS (CrewEnlisted) == (COUNT)~0
			|| LOBYTE (GLOBAL (CurrentActivity)) == IN_LAST_BATTLE
			|| LOBYTE (GLOBAL (CurrentActivity)) == WON_LAST_BATTLE)
		goto ExitUninitEncounter;

	if (GET_GAME_STATE (BATTLE_SEGUE) == 0)
	{
		ReinitQueue (&race_q[0]);
		ReinitQueue (&race_q[1]);
	}
	else
	{
		BOOLEAN Sleepy;
		DWORD Time;
		SIZE VictoryState;
		COUNT RecycleAmount = 0;
		SIZE i;
		RECT r, scavenge_r;
		TEXT t;
		STAMP ship_s;
		const UNICODE *str1 = NULL;
		const UNICODE *str2 = NULL;
		UNICODE buf[20];
		HSTARSHIP hStarShip;
		SHIP_FRAGMENTPTR FragPtr;
		COLOR fade_ship_cycle[] =
		{
			BUILD_COLOR (MAKE_RGB15 (0x7, 0x00, 0x00), 0x2F),
			BUILD_COLOR (MAKE_RGB15 (0xF, 0x00, 0x00), 0x2D),
			BUILD_COLOR (MAKE_RGB15 (0x17, 0x00, 0x00), 0x2B),
			BUILD_COLOR (MAKE_RGB15 (0x1F, 0xA, 0xA), 0x27),
			BUILD_COLOR (MAKE_RGB15 (0x1F, 0x14, 0x14), 0x25),
			BUILD_COLOR (MAKE_RGB15 (0x1F, 0x1F, 0x1F), 0x0F),
			BUILD_COLOR (MAKE_RGB15 (0x1F, 0x14, 0x14), 0x25),
			BUILD_COLOR (MAKE_RGB15 (0x1F, 0xA, 0xA), 0x27),
			BUILD_COLOR (MAKE_RGB15 (0x1B, 0x00, 0x00), 0x2A),
			BUILD_COLOR (MAKE_RGB15 (0x17, 0x00, 0x00), 0x2B),
		};
#define NUM_SHIP_FADES (sizeof (fade_ship_cycle) / \
		sizeof (fade_ship_cycle[0]))

		COUNT race_bounty[] =
		{
			RACE_SHIP_COST
		};

		SET_GAME_STATE (BATTLE_SEGUE, 0);
		SET_GAME_STATE (BOMB_CARRIER, 0);

		VictoryState = (
				HIBYTE (battle_counter) || !LOBYTE (battle_counter)
				|| GET_GAME_STATE (URQUAN_PROTECTING_SAMATRA)
				) ? 0 : 1;

		hStarShip = GetHeadLink (&GLOBAL (npc_built_ship_q));
		FragPtr = (SHIP_FRAGMENTPTR)LockStarShip (&GLOBAL (npc_built_ship_q),
				hStarShip);
		battle_counter = GET_RACE_ID (FragPtr) + 1;
		if (GetStarShipFromIndex (&GLOBAL (avail_race_q),
				(COUNT)(battle_counter - 1)) == 0)
		{
			VictoryState = -1;
			InitSISContexts ();
		}
		UnlockStarShip (&GLOBAL (npc_built_ship_q), hStarShip);

		Sleepy = TRUE;
		for (i = 0; i < NUM_SIDES; ++i)
		{
			PQUEUE pQueue;
			HSTARSHIP hNextShip;

			if (i == 0)
				pQueue = &GLOBAL (built_ship_q);
			else
			{
				if (VictoryState < 0)
					VictoryState = 0;
				else
				{
					UnlockMutex (GraphicsLock);
					DrawSISFrame ();
					LockMutex (GraphicsLock);

					DrawSISMessage (NULL_PTR);
					if (LOBYTE (GLOBAL (CurrentActivity)) == IN_HYPERSPACE)
						DrawHyperCoords (GLOBAL (ShipStamp.origin));
					else if (HIWORD (GLOBAL (ShipStamp.frame)) == 0)
						DrawHyperCoords (CurStarDescPtr->star_pt);
					else
						DrawSISTitle(GLOBAL_SIS (PlanetName));

					SetContext (SpaceContext);
					if (VictoryState)
						DrawArmadaPickShip (TRUE, &scavenge_r);
				}
				pQueue = &GLOBAL (npc_built_ship_q);
			}
				
			ReinitQueue (&race_q[(NUM_SIDES - 1) - i]);

			for (hStarShip = GetHeadLink (pQueue); hStarShip;
					hStarShip = hNextShip)
			{
				FragPtr = (SHIP_FRAGMENTPTR)LockStarShip (pQueue, hStarShip);
				hNextShip = _GetSuccLink (FragPtr);

				if (FragPtr->ShipInfo.crew_level == 0
						|| (VictoryState && i == NUM_SIDES - 1))
				{
					if (i == NUM_SIDES - 1)
					{
						++ships_killed;
						if (VictoryState)
						{
#define MAX_DEAD_DISPLAYED 5
							COUNT j;

							if (ships_killed == 1)
							{
								RecycleAmount = 0;

								DrawStatusMessage ((UNICODE *)~0);
								
								ship_s.origin.x = scavenge_r.corner.x + 32;
								ship_s.origin.y = scavenge_r.corner.y + 56;
								ship_s.frame = IncFrameIndex (
										FragPtr->ShipInfo.icons);
								DrawStamp (&ship_s);
								SetContextForeGroundColor (BUILD_COLOR (
										MAKE_RGB15 (0x8, 0x8, 0x8), 0x1F));
								SetContextFont (TinyFont);

								GetStringContents (
										FragPtr->ShipInfo.race_strings,
										(STRINGPTR)buf, FALSE);
								wstrupr (buf);

								t.baseline.x = scavenge_r.corner.x + 100;
								t.baseline.y = scavenge_r.corner.y + 68;
								t.align = ALIGN_CENTER;
								t.pStr = buf;
								t.CharCount = (COUNT)~0;
								font_DrawText (&t);
								t.baseline.y += 6;
								t.pStr = GAME_STRING (
										ENCOUNTER_STRING_BASE + 3);
								t.CharCount = (COUNT)~0;
								font_DrawText (&t);

								ship_s.frame = FragPtr->ShipInfo.icons;

								SetContextFont (MicroFont);
								str1 = GAME_STRING (
										ENCOUNTER_STRING_BASE + 4);
								str2 = GAME_STRING (
										ENCOUNTER_STRING_BASE + 5),
								DrawFadeText (str1, str2, TRUE, &scavenge_r);
							}

							r.corner.y = scavenge_r.corner.y + 9;
							r.extent.height = 22;

							SetContextForeGroundColor (BLACK_COLOR);

							r.extent.width = 34;
							r.corner.x = scavenge_r.corner.x +
									scavenge_r.extent.width
									- (10 + r.extent.width);
							DrawFilledRectangle (&r);

							/* collect bounty ResUnits */
							j = race_bounty[battle_counter - 1] >> 3;
							RecycleAmount += j;
							wsprintf (buf, "%u", RecycleAmount);
							t.baseline.x = r.corner.x + r.extent.width - 1;
							t.baseline.y = r.corner.y + 14;
							t.align = ALIGN_RIGHT;
							t.pStr = buf;
							t.CharCount = (COUNT)~0;
							SetContextForeGroundColor (BUILD_COLOR (
										MAKE_RGB15 (0x00, 0x00, 0x18), 0x50));
							font_DrawText (&t);
							DeltaSISGauges (0, 0, j);

							if ((VictoryState++ - 1) % MAX_DEAD_DISPLAYED)
								ship_s.origin.x += 17;
							else
							{
								SetContextForeGroundColor (BLACK_COLOR);

								r.corner.x = scavenge_r.corner.x + 10;
								r.extent.width = 104;
								DrawFilledRectangle (&r);

								ship_s.origin.x = r.corner.x + 2;
								ship_s.origin.y = scavenge_r.corner.y + 12;
							}

							if (Sleepy)
							{
								Time = GetTimeCounter ();
								for (j = 0; j < NUM_SHIP_FADES; ++j)
								{
									UnlockMutex (GraphicsLock);
									Sleepy = (BOOLEAN)!AnyButtonPress (TRUE);
									LockMutex (GraphicsLock);
									if (!Sleepy)
										break;

									SetContextForeGroundColor (fade_ship_cycle[j]);
									DrawFilledStamp (&ship_s);

									SleepThreadUntil (Time + (ONE_SECOND / 15));
									Time = GetTimeCounter ();
								}
							}
							DrawStamp (&ship_s);
						}
					}

					UnlockStarShip (pQueue, hStarShip);
					RemoveQueue (pQueue, hStarShip);
					FreeStarShip (pQueue, hStarShip);

					continue;
				}

				UnlockStarShip (pQueue, hStarShip);
			}
		}

		if (VictoryState)
		{
#ifdef NEVER
			DestroyDrawable (ReleaseDrawable (s.frame));
#endif /* NEVER */

			FlushInput ();
			Time = GetTimeCounter () + (ONE_SECOND * 3);
			UnlockMutex (GraphicsLock);
			while (!(AnyButtonPress (TRUE)) && GetTimeCounter () < Time)
				TaskSwitch ();
			LockMutex (GraphicsLock);
			if (!CurrentInputState.key[KEY_P1_ESCAPE])
			{
				DrawFadeText (str1, str2, FALSE, &scavenge_r);
				if (!CurrentInputState.key[KEY_P1_ESCAPE])
				{
					SetContextForeGroundColor (BLACK_COLOR);
					r.corner.x = scavenge_r.corner.x + 10;
					r.extent.width = 132;
					DrawFilledRectangle (&r);
					wsprintf (buf, "%u RU", RecycleAmount);
					t.baseline.x = r.corner.x + (r.extent.width >> 1);
					t.baseline.y = r.corner.y + 14;
					t.align = ALIGN_CENTER;
					t.pStr = buf;
					t.CharCount = (COUNT)~0;
					SetContextForeGroundColor (
							BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0x18), 0x50));
					font_DrawText (&t);

					str1 = GAME_STRING (ENCOUNTER_STRING_BASE + 6);
					str2 = GAME_STRING (ENCOUNTER_STRING_BASE + 7);
					DrawFadeText (str1, str2, TRUE, &scavenge_r);
					Time = GetTimeCounter () + ONE_SECOND * 2;
					UnlockMutex (GraphicsLock);
					while (!(AnyButtonPress (TRUE))
							&& GetTimeCounter () < Time)
						TaskSwitch ();
					LockMutex (GraphicsLock);
					if (!CurrentInputState.key[KEY_P1_ESCAPE])
						DrawFadeText (str1, str2, FALSE, &scavenge_r);
				}
			}

			DrawStatusMessage (NULL_PTR);
		}

		if (ships_killed && (battle_counter - 1) == THRADDASH_SHIP
				&& !GET_GAME_STATE (THRADD_MANNER))
		{
			if ((ships_killed += GET_GAME_STATE (THRADDASH_BODY_COUNT)) >
					THRADDASH_BODY_THRESHOLD)
				ships_killed = THRADDASH_BODY_THRESHOLD;
			SET_GAME_STATE (THRADDASH_BODY_COUNT, ships_killed);
		}
	}
ExitUninitEncounter:
	UnlockMutex (GraphicsLock);

	return (ships_killed);
}

void
EncounterBattle (void)
{
	MEM_HANDLE hLastIndex;
	ACTIVITY OldActivity;
	extern UWORD nth_frame;

	LockMutex (GraphicsLock);

	SET_GAME_STATE (BATTLE_SEGUE, 1);

	hLastIndex = SetResourceIndex (hResIndex);

	OldActivity = GLOBAL (CurrentActivity);
	if (LOBYTE (OldActivity) == IN_LAST_BATTLE)
		GLOBAL (CurrentActivity) = MAKE_WORD (IN_LAST_BATTLE, 0);
	else
		GLOBAL (CurrentActivity) = MAKE_WORD (IN_ENCOUNTER, 0);

//    FreeSC2Data ();
//    DestroyFont (ReleaseFont (MicroFont));
	WaitForSoundEnd (TFBSOUND_WAIT_ALL);
//    DestroySound (ReleaseSound (MenuSounds));

	if (GLOBAL (glob_flags) & CYBORG_ENABLED)
	{
		BYTE cur_speed;

		cur_speed = (BYTE)(GLOBAL (glob_flags) & COMBAT_SPEED_MASK)
				>> COMBAT_SPEED_SHIFT;
		if (cur_speed == 1)
			cur_speed = 0; /* normal speed */
		else if (cur_speed == 2)
			++cur_speed;   /* 4x speed, 3 of 4 frames skipped */
		else /* if (cur_speed == 3) */
			cur_speed = (BYTE)~0; /* maximum speed - no rendering */
		nth_frame = MAKE_WORD (1, cur_speed);
		PlayerControl[0] = CYBORG_CONTROL | AWESOME_RATING;
		PlayerInput[0] = ComputerInput;
	}

	GameSounds = CaptureSound (LoadSound (GAME_SOUNDS));

	UnlockMutex (GraphicsLock);
	Battle ();
	LockMutex (GraphicsLock);

	DestroySound (ReleaseSound (GameSounds));
	GameSounds = 0;

	if (GLOBAL (CurrentActivity) & CHECK_ABORT)
		GLOBAL_SIS (CrewEnlisted) = (COUNT)~0;

	if (GLOBAL (glob_flags) & CYBORG_ENABLED)
	{
		nth_frame = MAKE_WORD (0, 0);
		PlayerControl[0] = HUMAN_CONTROL | STANDARD_RATING;
		PlayerInput[0] = HumanInput[0];
	}

	SetResourceIndex (hResIndex);
//    MicroFont = CaptureFont (
// LoadGraphic (MICRO_FONT)
// );
//    MenuSounds = CaptureSound (LoadSound (MENU_SOUNDS));
//    LoadSC2Data ();

	GLOBAL (CurrentActivity) = OldActivity;

	SetResourceIndex (hLastIndex);

	UnlockMutex (GraphicsLock);
}

