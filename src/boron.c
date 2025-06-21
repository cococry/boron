#include "battery.h"
#include "brightness.h"
#include "power.h"
#include <leif/color.h>
#include <leif/layout.h>
#include <leif/ui_core.h>
#include <leif/util.h>
#include <leif/widget.h>
#include <leif/win.h>
#include <podvig/podvig.h>
#include <ragnar/api.h>
#include <runara/runara.h>

#include <leif/ez_api.h>


static void searchbtnpress(lf_ui_state_t* ui, lf_widget_t* widget);

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xinerama.h>

static lf_button_t* 
utilbtn(const char* text, bool set_color, bool set_width);

static void soundwidget(lf_ui_state_t* ui);

#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org))) 
#define ATTR_RERTIES 100
#define ATTR_RETRY_DELAY 50000 // 50ms 

#include "util.h"
#include "config.h"
#include "snd.h"

state_t s;

static void initxstate(state_t* s);
static void printerror(const char* message);
static Atom getatom(const char* atomstr, Display* dpy);
static area_t getbararea(area_t barmon);
static void setbarclasshints(Window bar, Display* dpy);
static void setwintypedock(Window win, Display* dpy);
static void setewmhstrut(Window, area_t area, Display* dpy);
static lf_window_t createuiwin(state_t* s);

static void querydesktops(state_t* s);
static int32_t getcurrentdesktop(Display* dsp);
static char** getdesktopnames(Display* dsp, uint32_t* o_numdesktops);

static void vseperator(void);
static void evcallback(void* ev, lf_ui_state_t* ui);
static area_t getfocusmon(state_t* s);

void 
initxstate(state_t* s) {
  if(!s) return;
  memset(s, 0, sizeof(*s));
  if(!(s->dpy = XOpenDisplay(NULL))) {
    fail("cannot open X display.");
  }
  s->root = DefaultRootWindow(s->dpy);

  memset(s->ewmh_atoms, ewmh_atom_none, sizeof(s->ewmh_atoms));
  s->ewmh_atoms[ewmh_atom_desktop_names]    = getatom("_NET_DESKTOP_NAMES", s->dpy);
  s->ewmh_atoms[ewmh_atom_current_desktop]  = getatom("_NET_CURRENT_DESKTOP", s->dpy);
}

void 
printerror(const char* message) {
  fprintf(stderr, "boron: %s\n", message);
  exit(1);
}


Atom getatom(const char* atomstr, Display* display) {
  Atom atom = XInternAtom(display, atomstr, False);
  return atom;
}


area_t
getbararea(area_t barmon) {
  area_t bararea;

  switch(barmode) {
    case BarLeft: {
      bararea = (area_t){
        barmon.x + barmargin_horz,
        barmon.y + barmargin_vert,
        barsize, 
        barmon.height - (barmargin_vert * 2)
      };
      break;
    }
    case BarRight: {
      bararea = (area_t){
        barmon.x + barmon.width - barsize - barmargin_horz,
        barmon.y + barmargin_vert,
        barsize, 
        barmon.height - (barmargin_horz * 2)
      };
      break;
    }
    case BarTop: {
      bararea = (area_t){
        barmon.x + barmargin_horz,
        barmon.y + barmargin_vert,
        barmon.width - (barmargin_horz * 2),
        barsize
      };
      break;
    }
    case BarBottom: {
      bararea = (area_t){
        barmon.x + barmargin_horz,
        barmon.y + barmon.height - barsize -barmargin_vert,
        barmon.width - (barmargin_horz * 2),
        barsize
      };
      break;
    }
    default: {
      bararea = (area_t){0, 0, 0, 0};
      break;
    }
  }
  return bararea;
}


void 
setbarclasshints(Window bar, Display* dpy) {
  const char* resname = "boron";
  const char* resclass = "Boron";
  size_t len = strlen(resname) + strlen(resclass) + 2;  // +2 for the two null terminators

  char* class = (char*)malloc(len);
  if (!class) {
    return;
  }
  snprintf(class, len, "%s%c%s%c", resname, '\0', resclass, '\0');

  XChangeProperty(dpy, bar, XA_WM_CLASS, XA_STRING, 8, PropModeReplace, (unsigned char*)class, len);
  XFlush(dpy);

  free(class);
}

void 
setwintypedock(Window win, Display* dpy) {
  Atom wintype_atom = getatom("_NET_WM_WINDOW_TYPE", dpy);
  Atom docktype_atom = getatom("_NET_WM_WINDOW_TYPE_DOCK", dpy);
  XChangeProperty(dpy, win, wintype_atom, XA_ATOM, 32, PropModeReplace, (unsigned char*)&docktype_atom, 1);
  XFlush(dpy);  

}

void setewmhstrut(Window win, area_t area, Display* dpy) {
  Atom strutpartial_atom = XInternAtom(s.dpy, "_NET_WM_STRUT_PARTIAL", False);

  unsigned long strut[12] = {0};

  switch (barmode) {
    case BarLeft: {
      // Space reserved on the left
      strut[0] = area.width + barmargin_horz * 2;
      break;
    }
    case BarRight: {
      // Space reserved on the right
      strut[1] = area.width + barmargin_horz * 2;
      break;
    }
    case BarTop: {
      // Space reserved on the top
      strut[2] = area.height + barmargin_vert * 2;
      break;
    }
    case BarBottom: {
      // Space reserved on the bottom
      strut[3] = area.height + barmargin_vert * 2;
      break;
    }
  }

  // Left start Y
  strut[4] = area.y;
  // Left end Y
  strut[5] = area.y + area.height - barmargin_vert;
  // Right start Y
  strut[6] = area.y;
  // Right end Y
  strut[7] = area.y + area.height - barmargin_vert;
  // Top start X
  strut[8] = area.x;
  // Top end X
  strut[9] = area.x + area.width - barmargin_horz;
  // Top start X
  strut[10] = area.x;
  // Top end X
  strut[11] = area.x + area.width - barmargin_horz;

  XChangeProperty(s.dpy, win, strutpartial_atom, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)strut, 12);
  XFlush(s.dpy); 
}

lf_window_t
createuiwin(state_t* s) {
  area_t barmon = getfocusmon(s);
  s->bararea = getbararea(barmon);

  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_POS_X, s->bararea.x);
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_POS_Y, s->bararea.y); 
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_TRANSPARENT_FRAMEBUFFER, true); 
  lf_ui_core_set_window_flags(LF_WINDOWING_FLAG_X11_OVERRIDE_REDIRECT);
  lf_window_t win = lf_ui_core_create_window(s->bararea.width, s->bararea.height, "Boron Bar");
  XSelectInput(lf_win_get_x11_display(), DefaultRootWindow(lf_win_get_x11_display()), PropertyChangeMask);
  lf_win_set_event_cb(win, evcallback);

  return win;
}

int32_t 
compstrs(const void* a, const void* b) {
  return strcmp(*(const char **)a, *(const char **)b);
}


void 
querydesktops(state_t* s) {
  uint32_t numdesktopsbefore = s->numdesktops;
  uint32_t crntdesktopbefore = s->crntdesktop;
  s->desktopnames = getdesktopnames(lf_win_get_x11_display(), &s->numdesktops);
  s->crntdesktop = getcurrentdesktop(lf_win_get_x11_display());
  if((s->numdesktops != numdesktopsbefore || 
    s->crntdesktop != crntdesktopbefore)) { 
    if(s->div_desktops) {
      lf_component_rerender(s->ui, uidesktops);
    }
  }
}


void
cog_hover(lf_ui_state_t* ui, lf_widget_t* widget) {
  lf_style_widget_prop_color(
    ui, widget, color, 
    lf_color_dim(lf_color_from_hex(barcolorforeground), 120.0f)
  );
  lf_widget_set_prop(s.ui, widget, &widget->props.padding_left, 5);
  lf_widget_set_prop(s.ui, widget, &widget->props.padding_right, 10);
}

void 
cog_leave(lf_ui_state_t* ui, lf_widget_t* widget) {
  lf_component_rerender(ui, uidesktops);
}

void newdesktop(lf_ui_state_t* ui, lf_widget_t* widget) {
  rg_cmd_switch_desktop(s.numdesktops);
}
void uidesktops(lf_ui_state_t* ui) {
  s.div_desktops = lf_div(s.ui);
  lf_widget_set_pos_x_absolute_percent(lf_crnt(s.ui), 0);
  lf_widget_set_layout(lf_crnt(s.ui), LF_LAYOUT_HORIZONTAL);
  lf_widget_set_sizing(lf_crnt(s.ui), LF_SIZING_FIT_CONTENT);
  lf_widget_set_alignment(lf_crnt(s.ui), LF_ALIGN_CENTER_VERTICAL);
  bar_style_widget(s.ui, lf_crnt(s.ui));

  lf_div(s.ui);
  lf_widget_set_layout(lf_crnt(s.ui), LF_LAYOUT_HORIZONTAL);
  lf_widget_set_alignment(lf_crnt(s.ui), LF_ALIGN_CENTER_VERTICAL);
  lf_widget_set_sizing(lf_crnt(s.ui), LF_SIZING_FIT_CONTENT);
  lf_widget_set_margin(s.ui, lf_crnt(s.ui), 0);
  lf_widget_set_padding(s.ui, lf_crnt(s.ui), 0);

  lf_button_t* btn = utilbtn("", false, true);
  lf_style_widget_prop(s.ui, &btn->base, corner_radius_percent, 50); 
  
  lf_style_widget_prop(s.ui, &btn->base, margin_right, 15); 
  lf_style_widget_prop_color(s.ui, &btn->base, color, lf_color_dim(lf_color_from_hex(barcolorforeground), 20.0));
  btn->on_click = searchbtnpress;
  for(uint32_t i = 0; i < s.numdesktops; i++) {
    bar_desktop_design(s.ui, i, s.crntdesktop, s.desktopnames[i]);
  }


  lf_div_end(s.ui);


  
  lf_button_t* addbtn = lf_button(s.ui);
  addbtn->on_click = newdesktop;
  addbtn->base.visible = s.numdesktops < 8;
  lf_widget_set_padding(s.ui, lf_crnt(s.ui), 0);
  lf_style_widget_prop_color(s.ui, lf_crnt(s.ui), color, LF_NO_COLOR);
  lf_style_widget_prop_color(s.ui, lf_crnt(s.ui), text_color, LF_WHITE);
  lf_text_sized(s.ui, "+", 24);
  lf_button_end(s.ui);
  lf_div_end(s.ui);
}

void uicmds(lf_ui_state_t* ui) {
  s.div_cmds = lf_div(s.ui);
  lf_widget_set_layout(lf_crnt(s.ui), LF_LAYOUT_HORIZONTAL);
  lf_widget_set_sizing(lf_crnt(s.ui), LF_SIZING_FIT_CONTENT);
  lf_widget_set_alignment(lf_crnt(s.ui), LF_ALIGN_CENTER_VERTICAL);
  lf_widget_set_padding(s.ui, lf_crnt(s.ui), 0);
  lf_widget_set_margin(s.ui, lf_crnt(s.ui), 0);

  display_cmd(CmdDate);

  lf_div_end(s.ui);
}

void hoverutilbtn(lf_ui_state_t* ui, lf_widget_t* widget) {
  lf_style_widget_prop_color(ui, widget, color, lf_color_dim(lf_color_from_hex(barcolorforeground), 30.0));
}
void leaveutilbtn(lf_ui_state_t* ui, lf_widget_t* widget) {
  lf_component_rerender(ui, uiutil);
}

lf_button_t* utilbtn(const char* text, bool set_color, bool set_width) {
  lf_button_t* btn = lf_button(s.ui);
  if(set_color) {
  ((lf_button_t*)lf_crnt(s.ui))->on_enter = hoverutilbtn;
  ((lf_button_t*)lf_crnt(s.ui))->on_leave = leaveutilbtn;
  }
  lf_widget_set_padding(s.ui, lf_crnt(s.ui), 0);
  if(set_width)
    lf_widget_set_fixed_width(s.ui, lf_crnt(s.ui), 30);
  if(set_color)
    lf_style_widget_prop_color(s.ui, lf_crnt(s.ui), color, LF_NO_COLOR);
  lf_style_widget_prop(s.ui, lf_crnt(s.ui), corner_radius_percent, 50);
  lf_text_t* txt = lf_text_h4(s.ui, text);
  lf_style_widget_prop_color(s.ui, lf_crnt(s.ui), text_color, lf_color_from_hex(barcolorforeground));
  lf_button_end(s.ui);
  return btn;
}

void togglepopup(pv_widget_t* popup) {
  if(popup->data.hidden) {
    pv_widget_show(popup);
  } else {
    pv_widget_hide(popup); 
  }
} 
void soundbtnpress(lf_ui_state_t* ui, lf_widget_t* widget) {
  togglepopup(s.sound_widget);
}
void brightnessbtnpress(lf_ui_state_t* ui, lf_widget_t* widget) {
  togglepopup(s.brightness_widget);
}
void searchbtnpress(lf_ui_state_t* ui, lf_widget_t* widget) {
  runcmd("~/.config/rofi/launchers/type-6/launcher.sh &");
}
void powerbtnpress(lf_ui_state_t* ui, lf_widget_t* widget) {
  togglepopup(s.poweroff_widget);
}

void btrybtnpresss(lf_ui_state_t* ui, lf_widget_t* widget) {
  togglepopup(s.battery_widget);
}

const char* getbatteryicon(void) {
  int total_percent = 0;
  int max_total = 0;
  int is_charging = 0;
  int valid_bats = 0;

  for (int i = 0; i < s.nbatteries; ++i) {
    if (strncmp(s.batteries[i].name, "BAT", 3) == 0) {
      if(s.batteries[i].last_percent == -1) {
        total_percent += 100;
      } else {
      total_percent += s.batteries[i].last_percent;
      }
      max_total += 100;
      if (s.batteries[i].status == BatteryStatusCharging) {
        is_charging = 1;
      }
      ++valid_bats;
    }
  }

  if (valid_bats == 0) {
    return "󱉝";
  }

  int combined_percent = (total_percent * 100) / max_total;

  const char* level = "";
  if (combined_percent >= 75)
    level = "";
  else if (combined_percent >= 50)
    level = "";
  else if (combined_percent >= 25)
    level = "";
  else if (combined_percent >= 5)
    level = "";

  const char* prefix = is_charging ? "󱐋" : "";

  static char icon[16];
  snprintf(icon, sizeof(icon), "%s%s", prefix, level);
  return icon;
}

void uiutil(lf_ui_state_t* ui) {
  s.div_util = lf_div(s.ui);
  lf_widget_set_pos_x_absolute_percent(lf_crnt(s.ui), 100);
  lf_widget_set_sizing(lf_crnt(s.ui), LF_SIZING_FIT_CONTENT);
  lf_widget_set_layout(lf_crnt(s.ui), LF_LAYOUT_HORIZONTAL);
  lf_widget_set_alignment(lf_crnt(s.ui), LF_ALIGN_CENTER_VERTICAL);

  {
    lf_button_t* btn = utilbtn("", true, true);
    btn->on_click = soundbtnpress;
  }
  if(s.have_backlight)
  {
    lf_button_t* btn = utilbtn("", true, true);
    btn->on_click = brightnessbtnpress;
  }

  {
    lf_button_t* btn = utilbtn(getbatteryicon(), true, false);
    btn->on_click = btrybtnpresss;
  }
  {
    char* icon =  "";
    if(s.sound_data.volume >= 50)    {  icon = ""; }
    else if(s.sound_data.volume > 0) {  icon = ""; } 
    else {  icon = ""; }
    lf_button_t* btn = utilbtn(icon,  true, true);
    btn->on_click = soundbtnpress;
  }
  {
    lf_button_t* btn = utilbtn("⏻", false, true);
    lf_style_widget_prop(s.ui, &btn->base, corner_radius_percent, 50);
    lf_style_widget_prop_color(s.ui, &btn->base, color, lf_color_dim(lf_color_from_hex(barcolorforeground), 20.0));
    btn->on_click = powerbtnpress;
  }


  lf_div_end(s.ui);
}

void vseperator(void) {
  float font_height = ((lf_text_t*)lf_crnt(s.ui))->font.pixel_size; 
  lf_div(s.ui);

  lf_widget_set_padding(s.ui, lf_crnt(s.ui), 0);
  lf_widget_set_fixed_width(s.ui, lf_crnt(s.ui), 2);
  lf_widget_set_fixed_height(s.ui, lf_crnt(s.ui), font_height);
  lf_style_widget_prop_color(s.ui, lf_crnt(s.ui), color, lf_color_from_hex(barcolorforeground) );
  lf_div_end(s.ui);
}

void evcallback(void* ev, lf_ui_state_t* ui) {
  (void)ui;
  XEvent* xev = (XEvent*)ev;
  switch(xev->type) {
    case PropertyNotify: 
      {
        XPropertyEvent *property_notify = (XPropertyEvent*)xev;

        if (property_notify->window == DefaultRootWindow(lf_win_get_x11_display())) {
          bool is_interesting = false;

          for (uint32_t i = 0; i < ewmh_atom_count; i++) {
            if (s.ewmh_atoms[i] == property_notify->atom) {
              is_interesting = true;
              break;
            }
          }

          if (is_interesting) {
            querydesktops(&s);
          }
        }
        break;
      }
  }
}

char** getdesktopnames(Display* dsp, uint32_t* o_numdesktops) {
  Atom netdesktopnames = XInternAtom(dsp, "_NET_DESKTOP_NAMES", False);
  Atom type;
  int format;
  unsigned long nitems, bytes;
  unsigned char *data = NULL;

  int retries = 0;
  while (retries < ATTR_RERTIES) {
    if (XGetWindowProperty(dsp, DefaultRootWindow(dsp), netdesktopnames,
                           0, 0x7FFFFFFF, False, XA_STRING, &type, &format,
                           &nitems, &bytes, &data) == Success && type == XA_STRING && format == 8) {
      break;
    }
    if (data) XFree(data);
    data = NULL;
    usleep(ATTR_RETRY_DELAY);  
    retries++;
  }

  if (!data) {
    printerror("Failed to get _NET_DESKTOP_NAMES property of root window.\n");
    return NULL;
  }

  char *data_ptr = (char *)data;
  int count = 0;
  for (char *p = data_ptr; p < data_ptr + nitems; ++p) {
    if (*p == '\0') {
      ++count;
    }
  }

  char **names = (char **)malloc(count * sizeof(char *));
  if (names == NULL) {
    fail("malloc:\n");
    XFree(data);
    return NULL;
  }

  int index = 0;
  char *name_start = data_ptr;
  for (char *p = data_ptr; p < data_ptr + nitems; ++p) {
    if (*p == '\0') {
      names[index] = strndup(name_start, p - name_start);
      if (names[index] == NULL) {
        fail("strndup:\n");
        for (int j = 0; j < index; ++j) {
          free(names[j]);
        }
        free(names);
        XFree(data);
        return NULL;
      }
      ++index;
      name_start = p + 1;
    }
  }

  XFree(data);
  *o_numdesktops = count;
  return names;
}


int32_t
getcurrentdesktop(Display* dsp) {
  Atom netcurrentdesktop;
  Atom type;
  int format;
  unsigned long nitems, bytes;
  unsigned char* data;
  netcurrentdesktop = XInternAtom(dsp, "_NET_CURRENT_DESKTOP", False);

  int retries = 0;
  bool success = false;
  while (retries < ATTR_RERTIES) {
    if (XGetWindowProperty(dsp, DefaultRootWindow(dsp), 
                           netcurrentdesktop, 0, 1, False, XA_CARDINAL,
                           &type, &format, 
                           &nitems, &bytes, &data) == Success 
      && type == XA_CARDINAL && nitems == 1) {
      success = true;
      break;
    }
    if (data) XFree(data);
    data = NULL;
    usleep(ATTR_RETRY_DELAY);  
    retries++;
  }
  if(!data) {
    fprintf(stderr, "Failed to get _NET_CURRENT_DESKTOP property of root window.\n");
    return 0;
  }

  int currentdesktop = *(long*)data;
  XFree(data);
  return currentdesktop;
}


area_t getfocusmon(state_t* s) {
  /* Taken from dmenu */
  int monx, mony, monw, monh; 
  Window rootret;
  uint32_t selectedmon;
  int32_t nmons;
  XineramaScreenInfo* screeninfo = XineramaQueryScreens(s->dpy, &nmons);
  int32_t greatestarea = 0;
  if(!screeninfo) {
    fail("cannot retrieve xinerama screens.");
  }
  Window focus;
  int32_t focusstate;
  XGetInputFocus(s->dpy, &focus, &focusstate);
  if (barmon >= 0 && barmon < nmons)
    selectedmon = barmon;
  else if (focus != s->root && focus != PointerRoot && focus != None) {
    Window* childs;
    uint32_t nchilds;
    Window focusparent;
    do {
      focusparent = focus;
      if (XQueryTree(s->dpy, focus, &rootret, &focus, &childs, &nchilds) && childs)
        XFree(childs);
    } while (focus != s->root && focus != focusparent);
    XWindowAttributes attr;
    if (!XGetWindowAttributes(s->dpy, focusparent, &attr)) {
      fail("failed to get window attributes of computed focus parent.");
    }
    for (uint32_t i = 0; i < nmons; i++) {
      int32_t area  = INTERSECT(attr.x, attr.y, attr.width, attr.height, screeninfo[i]);
      if (area > greatestarea) {
        greatestarea = area;
        selectedmon = i;
      }
    }
  }
  uint32_t maskreturn;
  int32_t cursorx, cursory;
  if (barmon < 0 && !greatestarea && XQueryPointer(
    s->dpy, s->root, &rootret, &rootret,
    &cursorx, &cursory, &focusstate, &focusstate, &maskreturn)) {
    for (uint32_t i = 0; i < nmons; i++) {
      if (INTERSECT(cursorx, cursory, 1, 1, screeninfo[i]) != 0) {
        selectedmon = i;
        break;
      }
    }
  }

  monx = screeninfo[selectedmon].x_org;
  mony = screeninfo[selectedmon].y_org; 
  monw = screeninfo[selectedmon].width;
  monh = screeninfo[selectedmon].height;
  XFree(screeninfo);

  return (area_t){
    monx, mony, 
    monw, monh
  };
}

void finish_cmd_timer(lf_ui_state_t* ui, lf_timer_t* timer) {
  uint32_t cmdidx = *(uint32_t*)timer->user_data;
  s.cmdoutputs[cmdidx] = getcmdoutput(barcmds[cmdidx].cmd);
  lf_component_rerender(ui, uicmds);
}


int main(void) {
  memset(&s, 0, sizeof(s));
  memset(&s.sound_data, 0, sizeof(s.sound_data));
  initxstate(&s);
  if(lf_windowing_init() != 0) {
    fail("cannot initialize libleif windowing backend.");
  }
  s.have_backlight = true;
  s.have_battery = true;
  s.have_popup_anims = true;

  if(!sndsetup()) return 1;
  if(!btrysetup()) return 1;
  if(!brightnesssetup()) {
    printf("boron: problem with initializing backlight driver.\n");
    s.have_backlight = false;
  };
  
  lf_window_t win = createuiwin(&s);

  setbarclasshints(win, s.dpy);
  setewmhstrut(win, s.bararea, s.dpy);
  setwintypedock(win, s.dpy);

  s.ui = lf_ui_core_init(win);

  lf_widget_set_font_family(s.ui, s.ui->root, barfont);
  lf_widget_set_font_style(s.ui, s.ui->root, LF_FONT_STYLE_REGULAR);

  lf_style_widget_prop_color(s.ui, lf_crnt(s.ui), color, 
                             ((lf_color_t){
                             .r = lf_color_from_hex(barcolor_window).r,
                             .g = lf_color_from_hex(barcolor_window).g,
                             .b = lf_color_from_hex(barcolor_window).b,
                             .a = bar_alpha 
                             }));

  s.brightness = 75;
  s.nightshift = 50;

  if(!sndcreatewidget(win)) return 1;
  if(!btrycreatewidget(win)) return 1;
  if(!powercreatewidget(win)) return 1;
  //if(!brightnesscreatewidget(win)) return 1;

   uint32_t ncmds = (sizeof barcmds / sizeof barcmds[0]);

  s.cmdoutputs = malloc(sizeof(char*) * ncmds);
  for(uint32_t i = 0; i < ncmds; i++) {
    if(barcmds[i].update_interval_secs > 0.0f) {
      lf_timer_t* timer = lf_ui_core_start_timer_looped(s.ui, barcmds[i].update_interval_secs, finish_cmd_timer);
      uint32_t* user_data = malloc(sizeof(uint32_t));
      *user_data = i;
      timer->user_data = user_data; 
    }

    s.cmdoutputs[i] = getcmdoutput(barcmds[i].cmd);
  }

  querydesktops(&s);

  bar_layout(s.ui);

  lf_widget_invalidate_size_and_layout(s.ui->root);
  lf_widget_shape(s.ui, s.ui->root);
  lf_widget_invalidate_size_and_layout(s.ui->root);
  lf_widget_shape(s.ui, s.ui->root);

  while(s.ui->running) {
    pv_update(s.pvstate);
    lf_ui_core_next_event(s.ui);
  }


  snd_mixer_selem_id_free(s.sndsid_master);
  snd_mixer_selem_id_free(s.sndsid_capture);
  snd_mixer_close(s.sndhandle);

  lf_ui_core_terminate(s.ui);

  return EXIT_SUCCESS;
}
