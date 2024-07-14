#pragma once

#include <stdbool.h>
#include <leif/leif.h>

#include "structs.h"

static const barmode_t  barmode         = BarLeft; 
static const uint32_t   barmargin       = 10;
static const uint32_t   barsize         = 35;
static const uint32_t   barborderwidth  = 1;
static const uint32_t   barbordercolor  = 0x555555;
static const uint32_t   barcolor        = 0x181818;
static const uint32_t   barcolor_desktop_focused = 0xfbf1c7;
static const uint32_t   barcolor_desktop_focused_font = 0x282828;
static const bool       bartransparent  = false; 
static const uint32_t   bartextcolor    = 0xeeeeee; 
static const char*      barfontpath     = "/usr/share/fonts/TTF/LilexNerdFont-Bold.ttf";
static const uint32_t   barfontsize     = 20;
