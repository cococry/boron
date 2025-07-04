#include "power.h"

#include "config.h"
#include "util.h"
#include <leif/color.h>
#include <leif/ez_api.h>
#include <leif/layout.h>
#include <leif/ui_core.h>
#include <leif/widget.h>
#include <leif/widgets/button.h>
#include <podvig/podvig.h>
#include <ragnar/api.h>
extern state_t s;


void rebootclick(lf_ui_state_t* ui, lf_widget_t* widget) {
  pv_widget_hide(s.poweroff_widget);
  runcmd("reboot");
}
void poweroffclick(lf_ui_state_t* ui, lf_widget_t* widget) {
  pv_widget_hide(s.poweroff_widget);
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
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 10);
  lf_style_widget_prop_color(ui, lf_crnt(ui), border_color, lf_color_from_hex(0x1c1c1c));
  lf_style_widget_prop(ui, lf_crnt(ui), border_width, 2);
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_from_hex(barcolorbackground));
  
  lf_text_h4(ui, "System Power");
  lf_widget_set_font_style(ui, lf_crnt(ui), LF_FONT_STYLE_BOLD);
  lf_style_widget_prop(ui, lf_crnt(ui), margin_left, 10);

  // Use vertical layout
  lf_widget_set_layout(lf_crnt(ui), LF_LAYOUT_VERTICAL);

  struct {
    char* icon;
    char* label;
    void (*handler)(lf_ui_state_t* ui, lf_widget_t* widget);
    lf_color_t bg;
  } buttons[] = {
    { "",  "Power Off", poweroffclick, lf_color_from_hex(0x952c2d) },
    { "",  "Reboot",    rebootclick,    lf_color_from_hex(0x2f2f2f)},
    { "󰍃", "Logout",    logoutclick,   lf_color_from_hex(0x2f2f2f)}, 
    { "󰌾", "Lock",      lockclick,     lf_color_from_hex(0x2f2f2f)}, 
  };

  for (int i = 0; i < 4; i++) {
    lf_button_t* btn = lf_button(ui);
    lf_style_widget_prop_color(ui, lf_crnt(ui), color, i == 0 ? lf_color_from_hex(0xcccccc) : LF_NO_COLOR);
    lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, i == 0 ? LF_BLACK : LF_WHITE);
    lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 30);
    lf_widget_set_sizing(lf_crnt(ui), LF_SIZING_FIT_PARENT);
    lf_widget_set_padding(ui, lf_crnt(ui), 5);
    lf_widget_set_fixed_height(ui, lf_crnt(ui), 25);
    if(!ui->_ez._assignment_only) {
      btn->base._component_props = btn->base.props;
    }
    ((lf_button_t*)lf_crnt(ui))->hovered_props = btn->base._component_props;
    ((lf_button_t*)lf_crnt(ui))->hovered_props.color = buttons[i].bg; 
    lf_widget_set_transition_props(lf_crnt(ui), 0.2f, lf_ease_out_quad);
    ((lf_button_t*)lf_crnt(ui))->on_click = buttons[i].handler;


    // Icon
    lf_text_h3(ui, buttons[i].icon);
    lf_widget_set_margin(ui, lf_crnt(ui), 0);
    

    // Text
    lf_text_p(ui, buttons[i].label);
    lf_widget_set_margin(ui, lf_crnt(ui), 0);
    lf_style_widget_prop(ui, lf_crnt(ui), margin_left, 15);

    lf_grower(ui);
    lf_button_end(ui);
  }

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
