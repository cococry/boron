#pragma once 

#include <leif/color.h>
#include <leif/ez_api.h>
#include <leif/layout.h>
#include <leif/ui_core.h>
#include <leif/util.h>
#include <leif/widget.h>
#include <ragnar/api.h>
#include <stdint.h>
#include <stdbool.h>
#include <ragnar/api.h>
#include <podvig/podvig.h>

#include <alsa/asoundlib.h>

extern void uidesktops(lf_ui_state_t* ui);
extern void uicmds(lf_ui_state_t* ui);
extern void uiutil(lf_ui_state_t* ui);

#define MAX_NAME_LEN 256
  
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
  float update_interval_secs;
} barcmd_t;

typedef enum {
  ewmh_atom_none = 0,
  ewmh_atom_desktop_names,
  ewmh_atom_current_desktop,
  ewmh_atom_count
} ewmh_atom_t;

typedef struct {
  int32_t x, y;
  uint32_t width, height;
} area_t;

typedef struct {
  float volume, volume_before;
  float microphone, microphone_before;
  bool micmuted;
  bool volmuted;
} sound_data_t;

typedef enum {
  BatteryStatusUnknown = 0,
  BatteryStatusCharging,
  BatteryStatusDischarging,
  BatteryStatusFull,
  BatteryStatusNotCharging,
  BatteryStatusCount,
} battery_status_t;

typedef struct {
  char name[MAX_NAME_LEN];
  int32_t last_percent;
  battery_status_t status;
} battery_data_t;

#define MAX_BATTERIES 16

typedef struct {
  Display* dpy;
  Window root;
  lf_ui_state_t* ui;


  area_t bararea;

  uint32_t crntdesktop;
  char** desktopnames;
  uint32_t numdesktops;

  Atom ewmh_atoms[ewmh_atom_count];

  lf_div_t* div_desktops, *div_cmds, *div_util;

  char** cmdoutputs;

  pv_state_t* pvstate;
  
  snd_mixer_t *sndhandle;
  snd_mixer_selem_id_t *sndsid_master;
  snd_mixer_selem_id_t *sndsid_capture;
  snd_mixer_elem_t *sndelem_master;
  snd_mixer_elem_t *sndelem_capture;
  struct pollfd *sndpfds;
  int32_t sndpollcount;

  pv_widget_t* sound_widget, *battery_widget;

  sound_data_t sound_data; 

  battery_data_t batteries[MAX_BATTERIES];
  uint32_t nbatteries;
} state_t;

typedef enum {
    CmdDate = 0,
    CmdBattery,
    CmdCount 
} barcmd_id_t;

static barcmd_t barcmds[CmdCount] = {
  [CmdDate] =  {
    .cmd = "date +\"%b %e %H:%M\"",
    .update_interval_secs = 1.0f
  },
  [CmdBattery] =  {
    .cmd = "boron-battery",
    .update_interval_secs = 60.0f
  },
};

extern state_t s;

static const char*      barfont             = "Arimo Nerd Font";
static const int32_t    barmon              = -1;
static const barmode_t  barmode             = BarTop; 
static const uint32_t   barmargin_vert      = 20;
static const uint32_t   barmargin_horz      = 300;
static const uint32_t   barsize             = 50;
static const uint32_t   barborderwidth      = 0;
static const uint32_t   barbordercolor      = 0x0;
static const uint32_t   barcolor_window     = 0x0;
static const uint32_t   bar_alpha           = 0; 
static const uint32_t   barcolorforeground  = 0xffffff;
static const uint32_t   barcolorbackground  = 0x111111;


void bar_style_widget(lf_ui_state_t* ui, lf_widget_t* widget);

void display_cmd(uint32_t idx);

void bar_layout(lf_ui_state_t* ui);

void bar_desktop_hover(lf_ui_state_t* ui, lf_widget_t* widget);

void bar_desktop_leave(lf_ui_state_t* ui, lf_widget_t* widget);

void bar_desktop_click(lf_ui_state_t* ui, lf_widget_t* widget);

void bar_desktop_design(lf_ui_state_t* ui, uint32_t desktop, uint32_t crntdesktop, const char* name);
