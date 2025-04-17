#include "config.h"

void bar_style_widget(lf_ui_state_t* ui, lf_widget_t* widget) {
  lf_style_widget_prop_color(ui, widget, color, LF_NO_COLOR); 
  lf_widget_set_padding(ui, widget, 5);
  lf_style_widget_prop(ui, widget, padding_left, 20);
  lf_style_widget_prop(ui, widget, padding_right, 20);
}

void display_cmd(uint32_t idx) {
  lf_div(s.ui);
  lf_widget_set_padding(s.ui, lf_crnt(s.ui), 0);
  lf_widget_set_margin(s.ui, lf_crnt(s.ui), 0);
  lf_widget_set_layout(lf_crnt(s.ui), LF_LAYOUT_HORIZONTAL);
  lf_widget_set_sizing(lf_crnt(s.ui), LF_SIZING_FIT_CONTENT);
  lf_crnt(s.ui)->scrollable = false;
  lf_text_sized(s.ui, s.cmdoutputs[idx], 20);
  lf_widget_set_font_style(s.ui, lf_crnt(s.ui), LF_FONT_STYLE_BOLD);
  lf_style_widget_prop(s.ui, lf_crnt(s.ui), margin_top, 10);
  lf_style_widget_prop_color(s.ui, lf_crnt(s.ui), text_color, lf_color_from_hex(barcolorforeground));
  lf_style_widget_prop(s.ui, lf_crnt(s.ui), margin_right, 15); 
  lf_div_end(s.ui);
}

void bar_layout(lf_ui_state_t* ui) {
  lf_div(ui);
  lf_widget_set_fixed_height_percent(ui, lf_crnt(ui), 100.0f);
  lf_widget_set_alignment(lf_crnt(ui), LF_ALIGN_CENTER_VERTICAL);
  lf_widget_set_padding(ui, lf_crnt(ui), 0);
  lf_crnt(s.ui)->scrollable = false;
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_from_hex(barcolorbackground));
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 30); 

  lf_div(ui);
  lf_widget_set_layout(lf_crnt(ui), LF_LAYOUT_HORIZONTAL);
  lf_widget_set_alignment(lf_crnt(ui), LF_ALIGN_CENTER_VERTICAL);
 

  lf_component(ui, uidesktops);
  // Left align
  lf_widget_set_pos_x_absolute_percent(&s.div_desktops->base, 0);

  lf_component(ui, uicmds);
  // Center align
  lf_widget_set_pos_x_absolute_percent(&s.div_cmds->base, 50);

  lf_component(ui, uiutil);
  // Right align
  lf_widget_set_pos_x_absolute_percent(&s.div_util->base, 100);

  lf_div_end(ui);
}

void bar_desktop_hover(lf_ui_state_t* ui, lf_widget_t* widget) {
  lf_widget_set_prop(ui, widget, &widget->props.padding_left, 5);
  lf_widget_set_prop(ui, widget, &widget->props.padding_right,5);
  lf_widget_set_prop(ui, widget, &widget->props.padding_top, 5);
  lf_widget_set_prop(ui, widget, &widget->props.padding_bottom,5);
  lf_widget_set_visible(widget->childs[0], true);
  lf_style_widget_prop_color(
    ui, widget, color, 
    lf_color_dim(lf_color_from_hex(barcolorforeground), 150.0f)
  );
}

void bar_desktop_leave(lf_ui_state_t* ui, lf_widget_t* widget) {
  lf_component_rerender(s.ui, uidesktops); // Back to initial state
}

void bar_desktop_click(lf_ui_state_t* ui, lf_widget_t* widget) {
  rg_cmd_switch_desktop(*(int32_t*)widget->user_data);
}

void bar_desktop_design(lf_ui_state_t* ui, uint32_t desktop, uint32_t crntdesktop, const char* name) {
  int32_t dist = abs((int32_t)desktop - (int32_t)crntdesktop);
    lf_button(ui);
    lf_widget_set_padding(ui, lf_crnt(ui), 0);
    lf_widget_set_transition_props(lf_crnt(ui), 0.2f, lf_ease_out_quad);
    lf_style_widget_prop_color(
        ui, lf_crnt(ui), color,
        (desktop == crntdesktop ? 
         lf_color_dim(lf_color_from_hex(barcolorforeground), 100.0f): 
         lf_color_dim(lf_color_from_hex(barcolorforeground), MAX(100.0f - (dist * 10), 50.0f))
        ));

    lf_widget_set_fixed_width(
        ui, lf_crnt(ui), 
        desktop == crntdesktop ? 55 : 12);
    lf_widget_set_fixed_height(ui, lf_crnt(ui), desktop == crntdesktop ? 20 : 12);
    lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent,50.0);
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

