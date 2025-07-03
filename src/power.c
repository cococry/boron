#include "power.h"

#include "config.h"
#include "util.h"
#include <leif/color.h>
#include <leif/ez_api.h>
#include <leif/layout.h>
#include <leif/ui_core.h>
#include <leif/widget.h>
#include <podvig/podvig.h>
#include <ragnar/api.h>
extern state_t s;


void rebootclick(lf_ui_state_t* ui, lf_widget_t* widget) {
  pv_widget_hide(s.poweroff_widget);
  runcmd("reboot");
}
void poweroffclick(lf_ui_state_t* ui, lf_widget_t* widget) {
  pv_widget_hide(s.poweroff_widget);
  runcmd("poweroff");
  printf("Power off.\n");
}
void logoutclick(lf_ui_state_t* ui, lf_widget_t* widget) {
  pv_widget_hide(s.poweroff_widget);
  rg_cmd_terminate(0);
}
void lockclick(lf_ui_state_t* ui, lf_widget_t* widget) {
  pv_widget_hide(s.poweroff_widget);
}

void powerwidget(lf_ui_state_t* ui) {
  lf_div(ui)->base.scrollable = false;
 lf_widget_set_padding(ui, lf_crnt(ui), 15.0f);
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 20); 
  lf_style_widget_prop_color(ui, lf_crnt(ui), border_color, lf_color_from_hex(0x1c1c1c)); 
  lf_style_widget_prop(ui, lf_crnt(ui), border_width, 2); 

  lf_style_widget_prop_color(
    ui, lf_crnt(ui), color, lf_color_from_hex(barcolorbackground));

  lf_div(ui);
  lf_widget_set_alignment(lf_crnt(ui), LF_ALIGN_CENTER_HORIZONTAL);
  lf_widget_set_layout(lf_crnt(ui), LF_LAYOUT_HORIZONTAL);
  lf_button(ui);
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_from_hex(0x952c2d));
  lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, LF_WHITE); 
  lf_widget_set_transition_props(lf_crnt(ui), 0.2f, lf_ease_out_quad);
  lf_widget_set_fixed_width(ui, lf_crnt(ui), 30);
  lf_widget_set_fixed_height(ui, lf_crnt(ui), 30);
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 50.0);
  lf_widget_set_padding(ui, lf_crnt(ui), 20);
  ((lf_button_t*)lf_crnt(ui))->on_click = poweroffclick;
  lf_text_h2(ui, "");

  lf_button_end(ui);

  lf_button(ui);
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_from_hex(0x2f2f2f));
  lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, LF_WHITE); 
  lf_widget_set_transition_props(lf_crnt(ui), 0.2f, lf_ease_out_quad);
  lf_widget_set_fixed_width(ui, lf_crnt(ui), 30);
  lf_widget_set_fixed_height(ui, lf_crnt(ui), 30);
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 50.0);
  lf_widget_set_padding(ui, lf_crnt(ui), 20);
  ((lf_button_t*)lf_crnt(ui))->on_click = rebootclick;
  lf_text_h2(ui, "");

  lf_button_end(ui);
  lf_div_end(ui);


  lf_div(ui);
  lf_style_widget_prop(ui, lf_crnt(ui), margin_top, -15);
  lf_widget_set_alignment(lf_crnt(ui), LF_ALIGN_CENTER_HORIZONTAL);
  lf_widget_set_layout(lf_crnt(ui), LF_LAYOUT_HORIZONTAL);
  lf_button(ui);
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_from_hex(0x2f2f2f));
  lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, LF_WHITE); 
  lf_widget_set_transition_props(lf_crnt(ui), 0.2f, lf_ease_out_quad);
  lf_widget_set_fixed_width(ui, lf_crnt(ui), 30);
  lf_widget_set_fixed_height(ui, lf_crnt(ui), 30);
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 50.0);
  lf_widget_set_padding(ui, lf_crnt(ui), 20);
  ((lf_button_t*)lf_crnt(ui))->on_click = logoutclick;
  lf_text_h2(ui, "󰍃");

  lf_button_end(ui);

  lf_button(ui);
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_from_hex(0x2f2f2f));
  lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, LF_WHITE); 
  lf_widget_set_transition_props(lf_crnt(ui), 0.2f, lf_ease_out_quad);
  lf_widget_set_fixed_width(ui, lf_crnt(ui), 30);
  lf_widget_set_fixed_height(ui, lf_crnt(ui), 30);
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 50.0);
  lf_widget_set_padding(ui, lf_crnt(ui), 20);
  ((lf_button_t*)lf_crnt(ui))->on_click = lockclick;
  lf_text_h2(ui, "󰌾");

  lf_button_end(ui);
  lf_div_end(ui);

  lf_div_end(ui);
}

static void widgetclose(pv_widget_t* widget) {
  (void)widget;
  lf_component_rerender(s.ui, uiutil); 
}
bool 
powercreatewidget(lf_window_t barwin) {
  if(!s.pvstate)
    s.pvstate = pv_init();
  if(!s.pvstate) return false;
  s.poweroff_widget = pv_widget(
    s.pvstate, "boron_poweroff_popup", powerwidget,
    s.bararea.x + s.bararea.width - 250, 
    s.bararea.y + s.bararea.height + 10,
    250, 250);


  pv_widget_set_popup_of(s.pvstate, s.poweroff_widget, barwin);
  lf_widget_set_font_family(s.poweroff_widget->ui, s.poweroff_widget->ui->root, barfont);
  lf_widget_set_font_style(s.poweroff_widget->ui, s.poweroff_widget->ui->root, LF_FONT_STYLE_REGULAR);
  lf_style_widget_prop_color(s.poweroff_widget->ui, s.poweroff_widget->ui->root, color, LF_NO_COLOR); 

  pv_widget_hide(s.poweroff_widget);

  s.poweroff_widget->data.close_cb = widgetclose; 
  return true;
}
