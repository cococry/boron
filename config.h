#pragma once 

#include <leif/color.h>
#include <leif/ez_api.h>
#include <leif/layout.h>
#include <leif/ui_core.h>
#include <stdint.h>
#include <stdbool.h>

#include <ragnar/api.h>

extern void uidesktops(void);
extern void uicmds(void);
  
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
  Display* dpy;
  Window root;
  lf_ui_state_t* ui;


  area_t bararea;

  uint32_t crntdesktop;
  char** desktopnames;
  uint32_t numdesktops;

  Atom ewmh_atoms[ewmh_atom_count];

  lf_div_t* div_desktops;
} state_t;

static barcmd_t barcmds[] = {
  
  {
    .cmd ="date +\"%H:%M\"",
    .update_interval_secs = 1.0f
  }
};

static state_t s;

static const char*      barfont             = "JetBrainsMono Nerd Font";
static const int32_t    barmon              = -1;
static const barmode_t  barmode             = BarTop; 
static const uint32_t   barmargin           = 10;
static const uint32_t   barsize             = 50;
static const uint32_t   barborderwidth      = 0;
static const uint32_t   barcolor_window     = 0x000000;
static const uint32_t   bar_alpha           = 0; 
static const uint32_t   barcolor_primary    = 0x181616;
static const uint32_t   barcolor_secondary  = 0x1F1D1D;
static const uint32_t   bartextcolor        = 0x000000;


void bar_style_widget(lf_ui_state_t* ui, lf_widget_t* widget) {
  lf_style_widget_prop_color(ui, widget, color, lf_color_from_hex(barcolor_primary)); 
  lf_style_widget_prop_color(ui, widget, border_color, lf_color_from_hex(barcolor_secondary)); 
  lf_style_widget_prop(ui, widget, border_width, 3);
  lf_style_widget_prop(ui, widget, corner_radius, 20);
  lf_widget_set_padding(ui, widget, 5);
  lf_style_widget_prop(ui, widget, padding_left, 20);
  lf_style_widget_prop(ui, widget, padding_right, 20);
}

void bar_layout(lf_ui_state_t* ui) {

  lf_div(ui);
  lf_widget_set_fixed_height_percent(ui, lf_crnt(ui), 100.0f);
  lf_widget_set_alignment(lf_crnt(ui), AlignCenterVertical);
  lf_widget_set_padding(ui, lf_crnt(ui), 0);
  lf_crnt(s.ui)->scrollable = false;

  lf_div(ui);
  lf_widget_set_layout(lf_crnt(ui), LayoutHorizontal);
  lf_widget_set_alignment(lf_crnt(ui), AlignCenterVertical);
  lf_component(ui, uidesktops);

  lf_div(ui);
  lf_widget_set_layout(lf_crnt(ui), LayoutHorizontal);
  lf_widget_set_sizing(lf_crnt(ui), SizingGrow);
  lf_div_end(ui);

  lf_component(ui, uicmds);

  lf_div_end(ui);
}


void set_prop(lf_widget_t* widget, float* prop, float val) {
  lf_widget_add_animation(widget, prop, *prop, val, 0.2, lf_ease_out_quad);
}

void bar_desktop_hover(lf_ui_state_t* ui, lf_widget_t* widget) {
  lf_widget_set_prop(ui, widget, &widget->props.padding_left, 5);
  lf_widget_set_prop(ui, widget, &widget->props.padding_right,5);
  if(*(int32_t*)widget->user_data != s.crntdesktop) {
    lf_widget_set_prop_color(ui, widget, &widget->props.color, lf_color_dim(lf_color_from_hex(barcolor_secondary) , 0.5));
    lf_widget_set_prop_color(ui, widget->childs[0], &widget->childs[0]->props.text_color, LF_WHITE); 
  }
  lf_widget_set_fixed_width(ui, widget, 55);
  lf_widget_set_visible(widget->childs[0], true);
}

void bar_desktop_leave(lf_ui_state_t* ui, lf_widget_t* widget) {
  lf_component_rerender(s.ui, uidesktops); // Back to initial state
}

void bar_desktop_click(lf_ui_state_t* ui, lf_widget_t* widget) {
  rg_cmd_switch_desktop(*(int32_t*)widget->user_data);
}

void bar_desktop_design(lf_ui_state_t* ui, uint32_t desktop, uint32_t crntdesktop, const char* name) {
    lf_button(ui);
    lf_widget_set_padding(ui, lf_crnt(ui), 0);
    lf_widget_set_transition_props(lf_crnt(ui), 0.2f, lf_ease_out_quad);
    lf_style_widget_prop_color(ui, lf_crnt(ui), color,
                               (desktop == crntdesktop ? 
                               lf_color_dim(lf_color_from_hex(barcolor_secondary), 160.0f): 
                               lf_color_dim(lf_color_from_hex(barcolor_secondary), 110.0f )
                               ));
    
    lf_widget_set_fixed_width(ui, lf_crnt(ui), desktop == crntdesktop ? 55 : 20);
    lf_widget_set_fixed_height(ui, lf_crnt(ui), 20);
    lf_style_widget_prop(ui, lf_crnt(ui), corner_radius, 20 / 2.0);
    ((lf_button_t*)lf_crnt(ui))->on_enter = bar_desktop_hover;
    ((lf_button_t*)lf_crnt(ui))->on_leave = bar_desktop_leave;
    ((lf_button_t*)lf_crnt(ui))->on_click = bar_desktop_click;
    uint32_t* data = malloc(sizeof(uint32_t));
    *data = desktop;
    lf_crnt(ui)->user_data = data;

    lf_text_h4(ui, name);

    lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, LF_BLACK);
    lf_crnt(ui)->visible = desktop == crntdesktop;

    lf_button_end(ui);
}
