#include "battery.h"

#include "config.h"
#include <leif/ez_api.h>
#include <leif/widget.h>

extern state_t s;

static void btrywidget(lf_ui_state_t* ui);

void 
btrywidget(lf_ui_state_t* ui) {
  lf_div(ui)->base.scrollable = false;
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 10); 
  lf_style_widget_prop_color(ui, lf_crnt(ui), border_color, lf_color_from_hex(0xcccccc)); 
  lf_style_widget_prop(ui, lf_crnt(ui), border_width, 2); 
  lf_widget_set_fixed_height_percent(ui, lf_crnt(ui), 100.0f);
  lf_widget_set_padding(ui, lf_crnt(ui), 20.0f);
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_from_hex(barcolorbackground));
  lf_text_h1(ui, "Battery");
  lf_widget_set_font_style(ui, lf_crnt(ui), 
                           LF_FONT_STYLE_BOLD);
  lf_div_end(ui);
}

bool 
btrycreatewidget(lf_window_t barwin) {
  if(!s.pvstate)
    s.pvstate = pv_init();
  if(!s.pvstate) return false;
  s.battery_widget = pv_widget(
    s.pvstate, "boron_battery_popup", btrywidget,
    s.bararea.x + s.bararea.width - 300 - 50, 
    s.bararea.y + s.bararea.height + 10,
    300, 170);

  pv_widget_set_popup_of(s.pvstate, s.battery_widget, barwin);
  lf_widget_set_font_family(s.battery_widget->ui, s.battery_widget->ui->root, barfont);
  lf_widget_set_font_style(s.battery_widget->ui, s.battery_widget->ui->root, LF_FONT_STYLE_REGULAR);

  pv_widget_hide(s.battery_widget);
  pv_widget_set_animation(s.battery_widget, PV_WIDGET_ANIMATION_SLIDE_OUT_VERT, 0.2, lf_ease_out_cubic);

  return true;

}
