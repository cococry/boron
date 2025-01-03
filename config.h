#pragma once 

#include <stdint.h>
#include <stdbool.h>

typedef enum {
  BarLeft = 0,
  BarRight,
  BarTop,
  BarBottom
} barmode_t;

typedef enum {
  BarPosStart = 0,
  BarPosCenter,
  BarPosEnd
} barpos_t;

typedef struct {
  char* cmd;
  barpos_t pos;
} displayed_command_t;

static displayed_command_t cmds[] = {
  {
    .cmd = "echo \"󰣇 Cococry - MODUS\"",
    .pos = BarPosStart
  },
  {
    .cmd = "echo \"1 2 3 4\"",
    .pos = BarPosCenter
  },
  {
    .cmd = "echo \"  \"",
    .pos = BarPosEnd
  },
  {
    .cmd = "date +\"%a, %b %d %H:%M\"",
    .pos = BarPosEnd
  },
};

static const barmode_t  barmode         = BarTop; 
static const uint32_t   barmargin       = 0;
static const uint32_t   barsize         = 35;
static const uint32_t   barborderwidth  = 0;
static const uint32_t   barbordercolor  = 0x555555;
static const uint32_t   barcolor        = 0x111111;
static const uint32_t   barcolor_desktop_focused = 0xfbf1c7;
static const uint32_t   barcolor_desktop_focused_font = 0x282828;
static const bool       bartransparent  = false; 
static const uint32_t   bartextcolor    = 0xeeeeee; 
