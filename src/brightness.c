
#include <leif/win.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "brightness.h"
#include "config.h"
#include <leif/color.h>
#include <leif/ez_api.h>
#include <leif/layout.h>
#include <leif/widget.h>
#include <leif/widgets/text.h>
#include <X11/extensions/Xrandr.h>

#define SYS_BACKLIGHT_PATH "/sys/class/backlight"


typedef struct {
  FILE *brightness_file;
  int max_brightness;
} brightness_ctx_t;

typedef struct {
  RRCrtc crtc;
  XRRCrtcGamma* gamma;
} gamma_ctx_t;

static gamma_ctx_t gammactx;
static brightness_ctx_t ctx;

static void setbrightnesspercent(brightness_ctx_t *ctx, int percent);
static void applyredshift
(Display *dpy, RRCrtc crtc, XRRCrtcGamma *gamma, int size, float percent);

void 
handlebrightnessslider(lf_ui_state_t* ui, lf_widget_t* widget, float* val) {
  setbrightnesspercent(&ctx, *val);
}
void 
handlenightshiftslider(lf_ui_state_t* ui, lf_widget_t* widget, float* val) {
  if(!s.usingnightshift) return;
  applyredshift(lf_win_get_x11_display(), 
                gammactx.crtc, gammactx.gamma, gammactx.gamma->size, *val);
}
static lf_slider_t* brightnessslider(lf_ui_state_t* ui, float* val, const char* icon) {

  lf_slider_t* slider = lf_slider(ui, val, 5, 100); 
  lf_widget_set_sizing(lf_crnt(ui), LF_SIZING_GROW);
  slider->base._fixed_width = false;
  slider->base._fixed_height = false;
  slider->handle_props.color = LF_WHITE;
  slider->_initial_handle_props = slider->handle_props;
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_dim(lf_color_from_hex(barcolorforeground), 60));
  lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, lf_color_from_hex(barcolorforeground));
  slider->base.container.size.y = 15;
  slider->handle.size.x = 15;
  slider->handle.size.y = 15;
  slider->_initial_handle_props.corner_radius = 15.0f / 2;
  slider->_initial_handle_props.border_width = 0; 
  slider->handle_props = slider->_initial_handle_props;
  lf_widget_set_padding(ui, lf_crnt(ui), 7.5);

  lf_text_t* text = lf_text_p(ui, icon);
  lf_widget_set_margin(ui, lf_crnt(ui), 0);
  lf_style_widget_prop(ui, lf_crnt(ui), margin_left, 5);
  lf_widget_set_padding(ui, lf_crnt(ui), 0);
  lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, LF_BLACK);

  lf_slider_end(ui);

  return slider;
}
static void brightnesswidget(lf_ui_state_t* ui);

void togglenightshift(lf_ui_state_t* ui, lf_widget_t* widget) {
  s.usingnightshift = !s.usingnightshift;
  if(!s.usingnightshift) {
  applyredshift(lf_win_get_x11_display(), 
                gammactx.crtc, gammactx.gamma, gammactx.gamma->size, 0);
  } else {
  applyredshift(lf_win_get_x11_display(), 
                gammactx.crtc, gammactx.gamma, gammactx.gamma->size, s.nightshift);
  }
  lf_component_rerender(ui, brightnesswidget);
}
void brightnesswidget(lf_ui_state_t* ui) {
  lf_div(ui)->base.scrollable = false;
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 10); 
  lf_style_widget_prop_color(ui, lf_crnt(ui), border_color, lf_color_from_hex(0xcccccc)); 
  lf_style_widget_prop(ui, lf_crnt(ui), border_width, 2); 
  lf_widget_set_fixed_height_percent(ui, lf_crnt(ui), 100.0f);
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_from_hex(barcolorbackground));
  lf_widget_set_padding(ui, lf_crnt(ui), 15);

  lf_text_h4(ui, "Display");
  lf_widget_set_font_style(ui, lf_crnt(ui), LF_FONT_STYLE_BOLD);
  lf_div(ui);
  lf_widget_set_margin(ui, lf_crnt(ui), 0);
  lf_widget_set_padding(ui, lf_crnt(ui), 0);
  lf_widget_set_layout(lf_crnt(ui), LF_LAYOUT_HORIZONTAL);

  brightnessslider(ui, &s.brightness, "")->on_slide = handlebrightnessslider;
  lf_button(ui)->on_click = togglenightshift;
  lf_widget_set_padding(ui, lf_crnt(ui), 0);
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 50.0f);
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, 
                             s.usingnightshift ? LF_WHITE : lf_color_from_hex(0x555555));
  lf_text_h3(ui, "󰖔");
  lf_button_end(ui);

  lf_div_end(ui);
  
  lf_text_h4(ui, "Nightsift");
  lf_widget_set_font_style(ui, lf_crnt(ui), LF_FONT_STYLE_BOLD);
  lf_div(ui);
  lf_widget_set_margin(ui, lf_crnt(ui), 0);
  lf_widget_set_padding(ui, lf_crnt(ui), 0);
  lf_widget_set_layout(lf_crnt(ui), LF_LAYOUT_HORIZONTAL);

  lf_slider_t* slider = 
    brightnessslider(ui, &s.nightshift, "");
  slider->on_slide = handlenightshiftslider;
  lf_style_widget_prop(ui, slider->base.childs[0], margin_top, 2);
  lf_button(ui);
  lf_widget_set_padding(ui, lf_crnt(ui), 0);
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 50.0f);
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, LF_NO_COLOR);
  lf_text_h3(ui, "󰖔");
  lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, LF_NO_COLOR);
  lf_button_end(ui);

  lf_div_end(ui);


  lf_div_end(ui);
}

int initctx(brightness_ctx_t *ctx) {
  DIR *dir = opendir(SYS_BACKLIGHT_PATH);
  if (!dir) return -1;

  struct dirent *entry;
  char driver[256] = {0};

  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", SYS_BACKLIGHT_PATH, entry->d_name);

    struct stat st;
    if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
      strncpy(driver, entry->d_name, sizeof(driver) - 1);
      break;
    }
  }
  closedir(dir);

  if (driver[0] == '\0') return -1;

  char max_path[512], brightness_path[512];
  snprintf(max_path, sizeof(max_path), "%s/%s/max_brightness", SYS_BACKLIGHT_PATH, driver);
  snprintf(brightness_path, sizeof(brightness_path), "%s/%s/brightness", SYS_BACKLIGHT_PATH, driver);

  FILE *fmax = fopen(max_path, "r");
  if (!fmax) return -1;

  if (fscanf(fmax, "%d", &ctx->max_brightness) != 1) {
    fclose(fmax);
    return -1;
  }
  fclose(fmax);

  ctx->brightness_file = fopen(brightness_path, "w");
  if (!ctx->brightness_file) return -1;

  return 0;
}

int initgammactx(gamma_ctx_t* ctx) {
  Display* dpy = lf_win_get_x11_display();
  Window root = RootWindow(dpy, DefaultScreen(dpy));
  XRRScreenResources *res = XRRGetScreenResources(dpy, root);

  ctx->crtc = res->crtcs[0];
  ctx->gamma = XRRGetCrtcGamma(dpy, ctx->crtc);

  return 0;
}

void applyredshift(Display *dpy, RRCrtc crtc, XRRCrtcGamma *gamma, int size, float percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    float red_scale   = 1.0f;
    float green_scale = 1.0f - (percent / 100.0f) * 0.5f;
    float blue_scale  = 1.0f - (percent / 100.0f);

    for (int i = 0; i < size; i++) {
        float scale = i / (float)(size - 1);
        gamma->red[i]   = 65535 * scale * red_scale;
        gamma->green[i] = 65535 * scale * green_scale;
        gamma->blue[i]  = 65535 * scale * blue_scale;
    }

    XRRSetCrtcGamma(dpy, crtc, gamma);
}

void setbrightnesspercent(brightness_ctx_t *ctx, int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  int value = (percent * ctx->max_brightness) / 100;
  rewind(ctx->brightness_file);
  fprintf(ctx->brightness_file, "%d\n", value);
  fflush(ctx->brightness_file);
}

void cleanupctx(brightness_ctx_t *ctx) {
  if (ctx->brightness_file) fclose(ctx->brightness_file);
}

bool 
brightnesssetup(void) {
  if(initctx(&ctx) != 0) return false;
  if(initgammactx(&gammactx) != 0) return false;

  return true;
}

bool 
brightnesscreatewidget(lf_window_t barwin) {

  if(!s.pvstate)
    s.pvstate = pv_init();
  if(!s.pvstate) return false;
  s.brightness_widget = pv_widget(
    s.pvstate, "boron_brightness_popup", brightnesswidget,
    s.bararea.x + s.bararea.width - 400, 
    s.bararea.y + s.bararea.height + 10,
    400, 180);

  pv_widget_set_popup_of(s.pvstate, s.brightness_widget, barwin);
  lf_widget_set_font_family(s.brightness_widget->ui, s.brightness_widget->ui->root, barfont);
  lf_widget_set_font_style(s.brightness_widget->ui, s.brightness_widget->ui->root, LF_FONT_STYLE_REGULAR);

  pv_widget_hide(s.brightness_widget);
  if(s.have_popup_anims)
    pv_widget_set_animation(s.brightness_widget, PV_WIDGET_ANIMATION_SLIDE_OUT_VERT, 0.2, lf_ease_out_cubic);

  setbrightnesspercent(&ctx, s.brightness);
  applyredshift(lf_win_get_x11_display(), 
                gammactx.crtc, gammactx.gamma, gammactx.gamma->size, 0);
  return true;
}
