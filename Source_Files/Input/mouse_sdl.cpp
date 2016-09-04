/*

	Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
	and the "Aleph One" developers.
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html

*/

/*
 *  mouse_sdl.cpp - Mouse handling, SDL specific implementation
 *
 *  May 16, 2002 (Woody Zenfell):
 *      Configurable mouse sensitivity
 *      Semi-hacky scheme to let mouse buttons simulate keypresses
 */

#include "cseries.h"
#include <math.h>

#include "mouse.h"
#include "player.h"
#include "shell.h"
#include "preferences.h"
#include "screen.h"


// Global variables
static bool mouse_active = false;
static uint8 button_mask = 0;		// Mask of enabled buttons
static _fixed snapshot_delta_yaw, snapshot_delta_pitch, snapshot_delta_velocity;
static _fixed snapshot_delta_scrollwheel;
static int snapshot_delta_x, snapshot_delta_y;


/*
 *  Initialize in-game mouse handling
 */

void enter_mouse(short type)
{
	if (type != _keyboard_or_game_pad) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
		mouse_active = true;
		snapshot_delta_yaw = snapshot_delta_pitch = snapshot_delta_velocity = 0;
		snapshot_delta_scrollwheel = 0;
		snapshot_delta_x = snapshot_delta_y = 0;
		button_mask = 0;	// Disable all buttons (so a shot won't be fired if we enter the game with a mouse button down from clicking a GUI widget)
		recenter_mouse();
	}
}


/*
 *  Shutdown in-game mouse handling
 */

void exit_mouse(short type)
{
	if (type != _keyboard_or_game_pad) {
		SDL_SetRelativeMouseMode(SDL_FALSE);
		mouse_active = false;
	}
}


/*
 *  Calculate new center mouse position when screen size has changed
 */

void recenter_mouse(void)
{
	if (mouse_active) {
		MainScreenCenterMouse();
	}
}


/*
 *  Take a snapshot of the current mouse state
 */

void mouse_idle(short type)
{
	if (mouse_active) {
		// Calculate axis deltas
		float dx = snapshot_delta_x;
		float dy = -snapshot_delta_y;
		snapshot_delta_x = 0;
		snapshot_delta_y = 0;
		
		// Mouse inversion
		if (TEST_FLAG(input_preferences->modifiers, _inputmod_invert_mouse))
			dy = -dy;
		
		// scale input by sensitivity
		const float sensitivityScale = 1.f / (66.f * FIXED_ONE);
		dx *= sensitivityScale * input_preferences->sens_horizontal;
		dy *= sensitivityScale * input_preferences->sens_vertical;
		
		if(input_preferences->mouse_acceleration) {
			/* do nonlinearity */
			dx = (dx * fabs(dx)) * 4.f;
			dy = (dy * fabs(dy)) * 4.f;
		}
		
		// 1 dx unit = 1 * 2^ABSOLUTE_YAW_BITS * (360 deg / 2^ANGULAR_BITS)
		//           = 90 deg
		//
		// 1 dy unit = 1 * 2^ABSOLUTE_PITCH_BITS * (360 deg / 2^ANGULAR_BITS)
		//           = 22.5 deg
		
		// Largest dx for which both -dx and +dx can be represented in 1 action flags bitset
		const float dxLimit = 0.5f - 1.f / (1<<ABSOLUTE_YAW_BITS);  // 0.4921875 dx units (~44.30 deg)
		
		// Largest dy for which both -dy and +dy can be represented in 1 action flags bitset
		const float dyLimit = 0.5f - 1.f / (1<<ABSOLUTE_PITCH_BITS);  // 0.46875 dy units (~10.55 deg)
		
		dx = PIN(dx, -dxLimit, dxLimit);
		dy = PIN(dy, -dyLimit, dyLimit);
		
		snapshot_delta_yaw   = static_cast<_fixed>(dx * FIXED_ONE);
		snapshot_delta_pitch = static_cast<_fixed>(dy * FIXED_ONE);
	}
}


/*
 *  Return mouse state
 */

void test_mouse(short type, uint32 *flags, _fixed *delta_yaw, _fixed *delta_pitch, _fixed *delta_velocity)
{
	if (mouse_active) {
		*delta_yaw = snapshot_delta_yaw;
		*delta_pitch = snapshot_delta_pitch;
		*delta_velocity = snapshot_delta_velocity;

		snapshot_delta_yaw = snapshot_delta_pitch = snapshot_delta_velocity = 0;
	} else {
		*delta_yaw = 0;
		*delta_pitch = 0;
		*delta_velocity = 0;
	}
}


void
mouse_buttons_become_keypresses(Uint8* ioKeyMap)
{
		uint8 buttons = SDL_GetMouseState(NULL, NULL);
		uint8 orig_buttons = buttons;
		buttons &= button_mask;				// Mask out disabled buttons

        for(int i = 0; i < NUM_SDL_MOUSE_BUTTONS; i++) {
            ioKeyMap[AO_SCANCODE_BASE_MOUSE_BUTTON + i] =
                (buttons & SDL_BUTTON(i+1)) ? SDL_PRESSED : SDL_RELEASED;
        }
		ioKeyMap[AO_SCANCODE_MOUSESCROLL_UP] = (snapshot_delta_scrollwheel > 0) ? SDL_PRESSED : SDL_RELEASED;
		ioKeyMap[AO_SCANCODE_MOUSESCROLL_DOWN] = (snapshot_delta_scrollwheel < 0) ? SDL_PRESSED : SDL_RELEASED;
		snapshot_delta_scrollwheel = 0;

        button_mask |= ~orig_buttons;		// A button must be released at least once to become enabled
}

/*
 *  Hide/show mouse pointer
 */

void hide_cursor(void)
{
	SDL_ShowCursor(0);
}

void show_cursor(void)
{
	SDL_ShowCursor(1);
}


/*
 *  Get current mouse position
 */

void get_mouse_position(short *x, short *y)
{
	int mx, my;
	SDL_GetMouseState(&mx, &my);
	*x = mx;
	*y = my;
}


/*
 *  Mouse button still down?
 */

bool mouse_still_down(void)
{
	SDL_PumpEvents();
	Uint8 buttons = SDL_GetMouseState(NULL, NULL);
	return buttons & SDL_BUTTON_LMASK;
}

void mouse_scroll(bool up)
{
	if (up)
		snapshot_delta_scrollwheel += 1;
	else
		snapshot_delta_scrollwheel -= 1;
}

void mouse_moved(int delta_x, int delta_y)
{
	snapshot_delta_x += delta_x;
	snapshot_delta_y += delta_y;
}
