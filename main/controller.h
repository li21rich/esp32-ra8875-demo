#pragma once

#include <stdbool.h>

extern volatile bool toggle_mode_pressed;
extern volatile bool toggle_submode_pressed;

void Controller_Init(void);