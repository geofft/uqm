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

#if defined (GFXMODULE_SDL) && defined (HAVE_OPENGL)

#include "libs/graphics/sdl/opengl.h"
#include "bbox.h"
#include "2xscalers.h"

static SDL_Surface *format_conv_surf;
static SDL_Surface *scaled_display;
static SDL_Surface *scaled_transition;

static int ScreenFilterMode;
static unsigned int DisplayTexture;
static unsigned int TransitionTexture;
static BOOLEAN scanlines;
static BOOLEAN upload_transitiontexture = FALSE;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define R_MASK 0xff000000
#define G_MASK 0x00ff0000
#define B_MASK 0x0000ff00
#define A_MASK 0x000000ff
#else
#define R_MASK 0x000000ff
#define G_MASK 0x0000ff00
#define B_MASK 0x00ff0000
#define A_MASK 0xff000000
#endif


static SDL_Surface *
Create_Screen ()
{
	SDL_Surface *result = SDL_CreateRGBSurface(SDL_SWSURFACE, ScreenWidth, ScreenHeight, 32,
		R_MASK, G_MASK, B_MASK, 0x00000000);
	if (result == NULL)
	{
		fprintf (stderr, "Couldn't create screen buffer: %s\n", SDL_GetError());
		exit (-1);
	}
	return result;
}

int
TFB_GL_InitGraphics (int driver, int flags, int width, int height, int bpp)
{
	char VideoName[256];
	int i, videomode_flags, texture_width, texture_height;

	GraphicsDriver = driver;

	fprintf (stderr, "Initializing SDL (OpenGL).\n");
	if ((SDL_Init (SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) == -1))
	{
		fprintf (stderr, "Could not initialize SDL: %s.\n", SDL_GetError());
		exit(-1);
	}

	SDL_VideoDriverName (VideoName, sizeof (VideoName));
	fprintf (stderr, "SDL driver used: %s\n", VideoName);
	fprintf (stderr, "SDL initialized.\n");
	fprintf (stderr, "Initializing Screen.\n");

	ScreenWidth = 320;
	ScreenHeight = 240;
	ScreenWidthActual = width;
	ScreenHeightActual = height;

	switch (bpp) {
		case 15:
			SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 5);
			SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 5);
			SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 5);
			break;

		case 16:
			SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 5);
			SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 6);
			SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 5);
			break;

		case 24:
			SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 8);
			SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 8);
			SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 8);
			break;

		case 32:
			SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 8);
			SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 8);
			SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 8);
			break;
		default:
			break;
	}

	SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, bpp);
	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);

	videomode_flags = SDL_OPENGL;
	if (flags & TFB_GFXFLAGS_FULLSCREEN)
		videomode_flags |= SDL_FULLSCREEN;

	SDL_Video = SDL_SetVideoMode (ScreenWidthActual, ScreenHeightActual, 
		bpp, videomode_flags);
	if (SDL_Video == NULL)
	{
		fprintf (stderr, "Couldn't set OpenGL %ix%ix%i video mode: %s\n",
				ScreenWidthActual, ScreenHeightActual, bpp,
				SDL_GetError ());
		exit (-1);
	}
	else
	{
		fprintf (stderr, "Set the resolution to: %ix%ix%i\n",
				SDL_GetVideoSurface()->w, SDL_GetVideoSurface()->h,
				SDL_GetVideoSurface()->format->BitsPerPixel);

		fprintf (stderr, "OpenGL renderer: %s version: %s\n",
				glGetString (GL_RENDERER), glGetString (GL_VERSION));
	}

	for (i = 0; i < TFB_GFX_NUMSCREENS; i++)
	{
		SDL_Screens[i] = Create_Screen ();
	}

	SDL_Screen = SDL_Screens[0];
	TransitionScreen = SDL_Screens[2];

	format_conv_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, 0, 0, 32,
		R_MASK, G_MASK, B_MASK, A_MASK);
	if (format_conv_surf == NULL)
	{
		fprintf (stderr, "Couldn't create format_conv_surf: %s\n", SDL_GetError());
		exit(-1);
	}

	if (GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPT ||
		GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPTADV)
	{
		scaled_display = SDL_CreateRGBSurface(SDL_SWSURFACE, ScreenWidth * 2,
			ScreenHeight * 2, 32, R_MASK, G_MASK, B_MASK, 0x00000000);
		if (scaled_display == NULL)
		{
			fprintf (stderr, "Couldn't create scaled_display: %s\n", SDL_GetError());
			exit(-1);
		}		
		scaled_transition = SDL_CreateRGBSurface(SDL_SWSURFACE, ScreenWidth * 2,
			ScreenHeight * 2, 32, R_MASK, G_MASK, B_MASK, 0x00000000);
		if (scaled_transition == NULL)
		{
			fprintf (stderr, "Couldn't create scaled_transition: %s\n", SDL_GetError());
			exit(-1);
		}

		texture_width = 1024;
		texture_height = 512;
	}
	else
	{
		texture_width = 512;
		texture_height = 256;
	}

	// pre-compute the RGB->YUV transformations
	Scale_PrepYUV();

	if (GfxFlags & TFB_GFXFLAGS_SCALE_BILINEAR ||
		GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPT ||
		GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPTADV)
		ScreenFilterMode = GL_LINEAR;
	else
		ScreenFilterMode = GL_NEAREST;

	if (flags & TFB_GFXFLAGS_SCANLINES)
		scanlines = TRUE;
	else
		scanlines = FALSE;

	glViewport (0, 0, ScreenWidthActual, ScreenHeightActual);
	glClearColor (0,0,0,0);
	glClear (GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	SDL_GL_SwapBuffers ();
	glClear (GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glDisable (GL_DITHER);
	glDepthMask(GL_FALSE);

	glGenTextures (1, &DisplayTexture);
	glBindTexture (GL_TEXTURE_2D, DisplayTexture);
	glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, texture_width, texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	glGenTextures (1, &TransitionTexture);
	glBindTexture (GL_TEXTURE_2D, TransitionTexture);
	glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, texture_width, texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	return 0;
}

void TFB_GL_UploadTransitionScreen (void)
{
	upload_transitiontexture = TRUE;	
}

void
TFB_GL_ScanLines (void)
{
	int y;

	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	glBlendFunc (GL_DST_COLOR, GL_ZERO);
	glColor3f (0.85f, 0.85f, 0.85f);
	for (y = 0; y < ScreenHeightActual; y += 2)
	{
		glBegin (GL_LINES);
		glVertex2i (0, y);
		glVertex2i (ScreenWidthActual, y);
		glEnd ();
	}

	glBlendFunc (GL_DST_COLOR, GL_ONE);
	glColor3f (0.2f, 0.2f, 0.2f);
	for (y = 1; y < ScreenHeightActual; y += 2)
	{
		glBegin (GL_LINES);
		glVertex2i (0, y);
		glVertex2i (ScreenWidthActual, y);
		glEnd ();
	}
}

void
TFB_GL_DrawQuad (void)
{
	glBegin (GL_TRIANGLE_FAN);
	glTexCoord2f (0, 0);
	glVertex2i (0, 0);
	glTexCoord2f (ScreenWidth / 512.0f, 0);
	glVertex2i (ScreenWidthActual, 0);	
	glTexCoord2f (ScreenWidth / 512.0f, ScreenHeight / 256.0f);
	glVertex2i (ScreenWidthActual, ScreenHeightActual);
	glTexCoord2f (0, ScreenHeight / 256.0f);
	glVertex2i (0, ScreenHeightActual);
	glEnd ();
}

void 
TFB_GL_SwapBuffers (void)
{
	int fade_amount;
	int transition_amount;

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	glOrtho (0,ScreenWidthActual,ScreenHeightActual, 0, -1, 1);
	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();
		
	glBindTexture (GL_TEXTURE_2D, DisplayTexture);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ScreenFilterMode);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ScreenFilterMode);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glColor4f (1, 1, 1, 1);
	
	if (GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPT ||
		GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPTADV)
	{
		SDL_Rect updated;
		updated.x = TFB_BBox.region.corner.x;
		updated.y = TFB_BBox.region.corner.y;
		updated.w = TFB_BBox.region.extent.width;
		updated.h = TFB_BBox.region.extent.height;

		if (GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPT)
			Scale_BiAdaptFilter (SDL_Screen, scaled_display, &updated);
		else if (GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPTADV)
			Scale_BiAdaptAdvFilter (SDL_Screen, scaled_display, &updated);

		SDL_LockSurface (scaled_display);
		glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, ScreenWidth * 2, ScreenHeight * 2,
			GL_RGBA, GL_UNSIGNED_BYTE, scaled_display->pixels);
		SDL_UnlockSurface (scaled_display);
	}
	else
	{
		SDL_LockSurface (SDL_Screen);
		glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, ScreenWidth, ScreenHeight,
			GL_RGBA, GL_UNSIGNED_BYTE, SDL_Screen->pixels);
		SDL_UnlockSurface (SDL_Screen);
	}

	TFB_GL_DrawQuad ();

	transition_amount = TransitionAmount;
	if (transition_amount != 255)
	{
		float scale_x = (ScreenWidthActual / (float)ScreenWidth);
		float scale_y = (ScreenHeightActual / (float)ScreenHeight);

		glBindTexture(GL_TEXTURE_2D, TransitionTexture);
		
		if (upload_transitiontexture) 
		{
			if (GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPT ||
				GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPTADV)
			{
				SDL_Rect updated;
				updated.x = updated.y = 0;
				updated.w = ScreenWidth;
				updated.h = ScreenHeight;
				if (GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPT)
					Scale_BiAdaptFilter (TransitionScreen, scaled_transition, &updated);
				else if (GfxFlags & TFB_GFXFLAGS_SCALE_BIADAPTADV)
					Scale_BiAdaptAdvFilter (TransitionScreen, scaled_transition, &updated);

				SDL_LockSurface (scaled_transition);
				glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, ScreenWidth * 2, ScreenHeight * 2,
					GL_RGBA, GL_UNSIGNED_BYTE, scaled_transition->pixels);
				SDL_UnlockSurface (scaled_transition);
			}
			else
			{
				SDL_LockSurface (TransitionScreen);
				glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, ScreenWidth, ScreenHeight,
					GL_RGBA, GL_UNSIGNED_BYTE, TransitionScreen->pixels);
				SDL_UnlockSurface (TransitionScreen);
			}

			upload_transitiontexture = FALSE;
		}

		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ScreenFilterMode);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ScreenFilterMode);

		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f (1, 1, 1, (255 - transition_amount) / 255.0f);
		
		glScissor (
			(GLint) (TransitionClipRect.x * scale_x),
			(GLint) ((ScreenHeight - (TransitionClipRect.y + TransitionClipRect.h)) * scale_y),
			(GLsizei) (TransitionClipRect.w * scale_x),
			(GLsizei) (TransitionClipRect.h * scale_y)
		);
		
		glEnable (GL_SCISSOR_TEST);
		TFB_GL_DrawQuad ();
		glDisable (GL_SCISSOR_TEST);
	}

	fade_amount = FadeAmount;
	if (fade_amount != 255)
	{
		float c;

		if (fade_amount < 255)
		{
			c = fade_amount / 255.0f;
			glBlendFunc (GL_DST_COLOR, GL_ZERO);
		}
		else
		{
			c = (fade_amount - 255) / 255.0f;
			glBlendFunc (GL_ONE, GL_ONE);
		}

		glDisable (GL_TEXTURE_2D);
		glEnable (GL_BLEND);
		glColor4f (c, c, c, 1);

		TFB_GL_DrawQuad ();
	}

	if (GfxFlags & TFB_GFXFLAGS_SCANLINES)
		TFB_GL_ScanLines ();

	SDL_GL_SwapBuffers ();
}

SDL_Surface* 
TFB_GL_DisplayFormatAlpha (SDL_Surface *surface)
{
	return SDL_ConvertSurface (surface, format_conv_surf->format, surface->flags);
}

#endif
