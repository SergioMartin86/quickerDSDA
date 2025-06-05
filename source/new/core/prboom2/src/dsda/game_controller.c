//
// Copyright(C) 2022 by Ryan Krafnick
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DSDA Game Controller
//


#include "d_event.h"
#include "d_main.h"
#include "lprintf.h"

#include "dsda/args.h"
#include "dsda/configuration.h"

#include "game_controller.h"

static int use_game_controller;

typedef struct {
  int deadzone;
  int sensitivity;
} axis_t;

static int swap_analogs;

static const char* button_names[] = {
  [DSDA_CONTROLLER_BUTTON_A] = "pad a",
  [DSDA_CONTROLLER_BUTTON_B] = "pad b",
  [DSDA_CONTROLLER_BUTTON_X] = "pad x",
  [DSDA_CONTROLLER_BUTTON_Y] = "pad y",
  [DSDA_CONTROLLER_BUTTON_BACK] = "pad back",
  [DSDA_CONTROLLER_BUTTON_GUIDE] = "pad guide",
  [DSDA_CONTROLLER_BUTTON_START] = "pad start",
  [DSDA_CONTROLLER_BUTTON_LEFTSTICK] = "lstick",
  [DSDA_CONTROLLER_BUTTON_RIGHTSTICK] = "rstick",
  [DSDA_CONTROLLER_BUTTON_LEFTSHOULDER] = "pad l",
  [DSDA_CONTROLLER_BUTTON_RIGHTSHOULDER] = "pad r",
  [DSDA_CONTROLLER_BUTTON_DPAD_UP] = "dpad u",
  [DSDA_CONTROLLER_BUTTON_DPAD_DOWN] = "dpad d",
  [DSDA_CONTROLLER_BUTTON_DPAD_LEFT] = "dpad l",
  [DSDA_CONTROLLER_BUTTON_DPAD_RIGHT] = "dpad r",
  [DSDA_CONTROLLER_BUTTON_MISC1] = "misc 1",
  [DSDA_CONTROLLER_BUTTON_PADDLE1] = "paddle 1",
  [DSDA_CONTROLLER_BUTTON_PADDLE2] = "paddle 2",
  [DSDA_CONTROLLER_BUTTON_PADDLE3] = "paddle 3",
  [DSDA_CONTROLLER_BUTTON_PADDLE4] = "paddle 4",
  [DSDA_CONTROLLER_BUTTON_TOUCHPAD] = "touchpad",
  [DSDA_CONTROLLER_BUTTON_TRIGGERLEFT] = "pad lt",
  [DSDA_CONTROLLER_BUTTON_TRIGGERRIGHT] = "pad rt",
};

const char* dsda_GameControllerButtonName(int button) {
  if (button >= sizeof(button_names) || !button_names[button])
    return "misc";

  return button_names[button];
}

static float dsda_AxisValue(axis_t* axis) {
  return 0.0;
}

static void dsda_PollLeftStick(void) {
}

static void dsda_PollRightStick(void) {
}

static inline int PollButton(dsda_game_controller_button_t button)
{
  // This depends on enums having same values
  return 0;
}

void dsda_PollGameControllerButtons(void) {
}

void dsda_PollGameController(void) {
}

void dsda_InitGameControllerParameters(void) {
}

void dsda_InitGameController(void) {
}
