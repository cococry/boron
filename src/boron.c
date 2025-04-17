#include <leif/color.h>
#include <leif/layout.h>
#include <leif/ui_core.h>
#include <leif/util.h>
#include <leif/widget.h>
#include <leif/win.h>
#include <podvig/podvig.h>
#include <runara/runara.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>

#include <leif/ez_api.h>


#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xinerama.h>

typedef struct {
  float volume, volume_before;
  float microphone, microphone_before;
  bool micmuted;
  bool volmuted;
} sound_data_t;

static sound_data_t sound_data = {0};

static void soundwidget(lf_ui_state_t* ui);

#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org))) 
#define ATTR_RERTIES 100
#define ATTR_RETRY_DELAY 50000 // 50ms 

#include "config.h"

static void fail(const char *fmt, ...);
static void initxstate(state_t* s);
static void printerror(const char* message);
static Atom getatom(const char* atomstr, Display* dpy);
static area_t getbararea(area_t barmon);
static void setbarclasshints(Window bar, Display* dpy);
static void setwintypedock(Window win, Display* dpy);
static void setewmhstrut(Window, area_t area, Display* dpy);
static lf_window_t createuiwin(state_t* s);
static char* getcmdoutput(const char* command);

static void querydesktops(state_t* s);
static int32_t getcurrentdesktop(Display* dsp);
static char** getdesktopnames(Display* dsp, uint32_t* o_numdesktops);

static void vseperator(void);
static void evcallback(void* ev, lf_ui_state_t* ui);
static area_t getfocusmon(state_t* s);

void
fail(const char *fmt, ...)
{
  /* Taken from dmenu */
  va_list ap;
  int saved_errno;

  saved_errno = errno;

  va_start(ap, fmt);
  vfprintf(stderr, "boron: ", ap);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  if (fmt[0] && fmt[strlen(fmt)-1] == ':')
    fprintf(stderr, "%s", strerror(saved_errno));
  fputc('\n', stderr);

  exit(EXIT_FAILURE);
}

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
runcmd(const char* cmd) {
  if (cmd == NULL) {
    return;
  }

  pid_t pid = fork();
  if (pid == 0) {
    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
    fprintf(stderr, "boron: failed to execute command '%s'.\n", cmd);
    _exit(EXIT_FAILURE);
  } else if (pid > 0) {
    int32_t status;
    waitpid(pid, &status, 0);
    return;
  } else {
    fprintf(stderr, "boron: failed to execute command '%s'.\n", cmd);
    return;
  }
}

char*
getcmdoutput(const char* command) {
  FILE* fp;
  char* result = NULL;
  size_t result_size = 0;
  char buffer[1024];

  fp = popen(command, "r");
  if (fp == NULL) {
    printerror("failed to execute popen command.\n");
    return NULL;
  }
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    size_t buffer_len = strlen(buffer);
    char* new_result = realloc(result, result_size + buffer_len + 1);
    if (new_result == NULL) {
      printerror("failed to allocate memory to hold command output.\n");
      free(result);
      pclose(fp);
      return NULL;
    }
    result = new_result;
    memcpy(result + result_size, buffer, buffer_len);
    result_size += buffer_len;
    result[result_size] = '\0'; 
  }

  if (pclose(fp) == -1) {
    printerror("failed to close command file pointer.\n");
    free(result);
    return NULL;
  }

  return result;
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

  lf_text_h2(s.ui, "󰣇");
  lf_style_widget_prop(s.ui, lf_crnt(s.ui), margin_right, 20);
  for(uint32_t i = 0; i < s.numdesktops; i++) {
    bar_desktop_design(s.ui, i, s.crntdesktop, s.desktopnames[i]);
  }
  lf_div_end(s.ui);

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

lf_button_t* utilbtn(const char* text, bool set_color) {
  lf_button_t* btn = lf_button(s.ui);
  lf_style_widget_prop(s.ui, lf_crnt(s.ui), corner_radius_percent, 30);
  lf_widget_set_transition_props(lf_crnt(s.ui), 0.2f, lf_ease_out_quad);
  ((lf_button_t*)lf_crnt(s.ui))->on_enter = hoverutilbtn;
  ((lf_button_t*)lf_crnt(s.ui))->on_leave = leaveutilbtn;
  lf_widget_set_padding(s.ui, lf_crnt(s.ui), 0);
  lf_widget_set_fixed_width(s.ui, lf_crnt(s.ui), 30);
  if(set_color)
    lf_style_widget_prop_color(s.ui, lf_crnt(s.ui), color, LF_NO_COLOR);
  lf_text_t* txt = lf_text_h4(s.ui, text);
  lf_style_widget_prop_color(s.ui, lf_crnt(s.ui), text_color, lf_color_from_hex(barcolorforeground));
  lf_button_end(s.ui);
  return btn;
}

void soundbtnpress(lf_ui_state_t* ui, lf_widget_t* widget) {
  if(s.sound_widget->data.hidden) {
    pv_widget_show(s.sound_widget);
  } else {
    pv_widget_hide(s.sound_widget); 
  }
}

void uiutil(lf_ui_state_t* ui) {
  s.div_util = lf_div(s.ui);
  lf_widget_set_pos_x_absolute_percent(lf_crnt(s.ui), 100);
  lf_widget_set_sizing(lf_crnt(s.ui), LF_SIZING_FIT_CONTENT);
  lf_widget_set_layout(lf_crnt(s.ui), LF_LAYOUT_HORIZONTAL);
  lf_widget_set_alignment(lf_crnt(s.ui), LF_ALIGN_CENTER_VERTICAL);

  {
    lf_button_t* btn = utilbtn("", true);
    btn->on_click = soundbtnpress;
  }

  {
    lf_button_t* btn = utilbtn(s.cmdoutputs[CmdBattery], true);
    btn->on_click = soundbtnpress;
  }
  {
    char* icon =  "";
    if(sound_data.volume >= 50)    {  icon = ""; }
    else if(sound_data.volume > 0) {  icon = ""; } 
    else {  icon = ""; }
    lf_button_t* btn = utilbtn(icon,  true);
    btn->on_click = soundbtnpress;
  }
  {
    lf_button_t* btn = utilbtn("⏻", false);
    lf_style_widget_prop_color(s.ui, &btn->base, color, lf_color_dim(lf_color_from_hex(barcolorforeground), 20.0));
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

void handlevolumelsider(lf_ui_state_t* ui, lf_widget_t* widget, float* val) {
  if(sound_data.volmuted) {
    runcmd("amixer sset Master toggle &");
    sound_data.volmuted = false;
  }
  char buf[32];
  sprintf(buf, "amixer sset Master %f%% &", *val); 
  runcmd(buf);
  lf_component_rerender(s.sound_widget->ui, soundwidget);
  lf_component_rerender(s.ui, uiutil);
}

void handlemicrophoneslider(lf_ui_state_t* ui, lf_widget_t* widget, float* val) {
  char buf[32];
  sprintf(buf, "amixer sset Capture %f%% &", *val); 
  runcmd(buf);
  if(sound_data.micmuted) {
    runcmd("amixer sset Capture toggle &");
    sound_data.micmuted = false;
  }
  lf_component_rerender(s.sound_widget->ui, soundwidget);
}

lf_slider_t* volumeslider(lf_ui_state_t* ui, float* val){
  lf_slider_t* slider = lf_slider(ui, val, 0, 100);
  lf_widget_set_transition_props(lf_crnt(ui), 0.2f, lf_ease_out_quad);
  slider->handle_props.color = LF_WHITE;
  slider->_initial_handle_props = slider->handle_props;
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_dim(lf_color_from_hex(barcolorforeground), 60));
  lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, lf_color_from_hex(barcolorforeground));
  slider->base.container.size.y = 2;
  slider->handle.size.x = 15;
  slider->handle.size.y = 15;
  slider->handle_props.corner_radius = 15.0f / 2;
  lf_style_widget_prop(ui, lf_crnt(ui), margin_left, 20);
  return slider;
}


void mutemic(lf_ui_state_t* ui, lf_widget_t* widget) {
  char buf[32];
  sprintf(buf, "amixer sset Capture toggle"); 
  sound_data.micmuted = !sound_data.micmuted;
  if(sound_data.micmuted) {
    sound_data.microphone_before = sound_data.microphone;
    sound_data.microphone = 0;
  } else {
    sound_data.microphone = sound_data.microphone_before;
  }
  lf_component_rerender(s.sound_widget->ui, soundwidget); 
  runcmd(buf);
}

void mutevolume(lf_ui_state_t* ui, lf_widget_t* widget) {
  char buf[32];
  sprintf(buf, "amixer sset Master toggle"); 
  sound_data.volmuted = !sound_data.volmuted;
  if(sound_data.volmuted) {
    sound_data.volume_before = sound_data.volume;
    sound_data.volume = 0;
  } else {
    sound_data.volume = sound_data.volume_before;
  }
  lf_component_rerender(s.sound_widget->ui, soundwidget); 
  runcmd(buf);
}

void soundwidget(lf_ui_state_t* ui) {
  lf_div(ui)->base.scrollable = false;
  lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 10); 
  lf_style_widget_prop_color(ui, lf_crnt(ui), border_color, lf_color_from_hex(0xcccccc)); 
  lf_style_widget_prop(ui, lf_crnt(ui), border_width, 2); 
  lf_widget_set_fixed_height_percent(ui, lf_crnt(ui), 100.0f);
  lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_from_hex(barcolorbackground));

  lf_text_h4(ui, " System Volume");
  lf_widget_set_font_style(ui, lf_crnt(ui), LF_FONT_STYLE_BOLD);
  lf_div(ui);
  lf_widget_set_margin(ui, lf_crnt(ui), 0);
  lf_widget_set_padding(ui, lf_crnt(ui), 0);
  lf_widget_set_layout(lf_crnt(ui), LF_LAYOUT_HORIZONTAL);
  lf_widget_set_alignment(lf_crnt(ui), LF_ALIGN_CENTER_HORIZONTAL | LF_ALIGN_CENTER_VERTICAL);
  {
    char* icon =  "";
    if(sound_data.volume >= 50)    {  icon = ""; }
    else if(sound_data.volume > 0) {  icon = ""; } 
    else {  icon = ""; }
    lf_button(ui)->on_click = mutevolume;
    lf_crnt(ui)->props = ui->theme->text_props;

    lf_widget_set_fixed_width(ui, lf_crnt(ui), 35);
    lf_widget_set_fixed_height(ui, lf_crnt(ui), 30);
    lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_dim(lf_color_from_hex(barcolorforeground), 15.0f)); 
    lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 20.0f); 
    lf_text_h4(ui, icon); 
    lf_widget_set_fixed_width(ui, lf_crnt(ui), 18);
    lf_widget_set_fixed_height(ui, lf_crnt(ui), 15);
    lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, lf_color_from_hex(barcolorforeground));
    lf_button_end(ui);
  }
  volumeslider(ui, &sound_data.volume)->on_slide = handlevolumelsider;

  lf_div_end(ui);

  lf_text_h4(ui, " Microphone");
  lf_widget_set_font_style(ui, lf_crnt(ui), LF_FONT_STYLE_BOLD);
  lf_div(ui);
  lf_widget_set_margin(ui, lf_crnt(ui), 0);
  lf_widget_set_padding(ui, lf_crnt(ui), 0);
  lf_widget_set_layout(lf_crnt(ui), LF_LAYOUT_HORIZONTAL);
  lf_widget_set_alignment(lf_crnt(ui), LF_ALIGN_CENTER_HORIZONTAL | LF_ALIGN_CENTER_VERTICAL);
  {
    char* icon =  "";
    if(sound_data.microphone >= 50)     {  icon = ""; }
    else if(sound_data.microphone > 0)  {  icon = ""; } 
    else {  icon = ""; }
    lf_button(ui)->on_click = mutemic;
    lf_crnt(ui)->props = ui->theme->text_props;

    lf_widget_set_fixed_width(ui, lf_crnt(ui), 35);
    lf_widget_set_fixed_height(ui, lf_crnt(ui), 30);
    lf_style_widget_prop_color(ui, lf_crnt(ui), color, lf_color_dim(lf_color_from_hex(barcolorforeground), 15.0f)); 
    lf_style_widget_prop(ui, lf_crnt(ui), corner_radius_percent, 20.0f); 
    lf_text_h4(ui, icon); 
    lf_widget_set_fixed_width(ui, lf_crnt(ui), 18);
    lf_widget_set_fixed_height(ui, lf_crnt(ui), 15);
    lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, lf_color_from_hex(barcolorforeground));
    lf_button_end(ui);
  }

  lf_style_widget_prop_color(ui, lf_crnt(ui), text_color, lf_color_from_hex(barcolorforeground));
  volumeslider(ui, &sound_data.microphone)->on_slide = handlemicrophoneslider;

  lf_div_end(ui);


  lf_div_end(ui);
}

bool alsasetup(state_t* s) {
  const char* card = "default";
  const char* selem_name_master = "Master";
  const char* selem_name_capture = "Capture";

  if (snd_mixer_open(&s->sndhandle, 0) < 0) {
    fprintf(stderr, "boron: alsa: error opening mixer\n");
    return false;
  }

  if (snd_mixer_attach(s->sndhandle, card) < 0) {
    fprintf(stderr, "boron: alsa: error attaching mixer\n");
    snd_mixer_close(s->sndhandle);
    return false;
  }

  if (snd_mixer_selem_register(s->sndhandle, NULL, NULL) < 0) {
    fprintf(stderr, "boron: alsa: error registering mixer\n");
    snd_mixer_close(s->sndhandle);
    return false;
  }

  if (snd_mixer_load(s->sndhandle) < 0) {
    fprintf(stderr, "boron: alsa: error loading mixer elements\n");
    snd_mixer_close(s->sndhandle);
    return false;
  }

  // Master setup
  snd_mixer_selem_id_malloc(&s->sndsid_master);
  snd_mixer_selem_id_set_index(s->sndsid_master, 0);
  snd_mixer_selem_id_set_name(s->sndsid_master, selem_name_master);

  s->sndelem_master = snd_mixer_find_selem(s->sndhandle, s->sndsid_master);
  if (!s->sndelem_master) {
    fprintf(stderr, "boron: unable to find simple control '%s'\n", selem_name_master);
    snd_mixer_selem_id_free(s->sndsid_master);
    snd_mixer_close(s->sndhandle);
    return false;
  }

  // Capture setup
  snd_mixer_selem_id_malloc(&s->sndsid_capture);
  snd_mixer_selem_id_set_index(s->sndsid_capture, 0);
  snd_mixer_selem_id_set_name(s->sndsid_capture, selem_name_capture);

  s->sndelem_capture = snd_mixer_find_selem(s->sndhandle, s->sndsid_capture);
  if (!s->sndelem_capture) {
    fprintf(stderr, "boron: unable to find simple control '%s'\n", selem_name_capture);
    snd_mixer_selem_id_free(s->sndsid_master);
    snd_mixer_selem_id_free(s->sndsid_capture);
    snd_mixer_close(s->sndhandle);
    return false;
  }

  s->sndpollcount = snd_mixer_poll_descriptors_count(s->sndhandle);
  s->sndpfds = malloc(sizeof(*s->sndpfds) * s->sndpollcount);
  snd_mixer_poll_descriptors(s->sndhandle, s->sndpfds, s->sndpollcount);

  return true;
}

pthread_mutex_t sound_mutex = PTHREAD_MUTEX_INITIALIZER;
void* alsalisten(void *arg) {
  state_t* s = (state_t *)arg;

  while (1) {
    if (poll(s->sndpfds, s->sndpollcount, -1) > 0) {
      snd_mixer_handle_events(s->sndhandle);

      {
        // --- MASTER VOLUME (Playback) ---
        long min, max, vol;
        int pswitch;
        snd_mixer_selem_get_playback_volume_range(s->sndelem_master, &min, &max);
        snd_mixer_selem_get_playback_volume(s->sndelem_master, SND_MIXER_SCHN_FRONT_LEFT, &vol);
        snd_mixer_selem_get_playback_switch(s->sndelem_master, SND_MIXER_SCHN_FRONT_LEFT, &pswitch);
        bool volmuted_before = sound_data.volmuted;
        sound_data.volmuted = pswitch == 0;
        if (sound_data.volmuted) {
          sound_data.volume_before = sound_data.volume;
          sound_data.volume = 0;
          if(s->ui)
            lf_component_rerender(s->ui, uiutil);
        }
        if (volmuted_before != sound_data.volmuted) {
          pthread_mutex_lock(&sound_mutex);
          if (s->sound_widget) {
            lf_component_rerender(s->sound_widget->ui, soundwidget);

          }
          pthread_mutex_unlock(&sound_mutex);
        } 
        if (max - min > 0 && pswitch != 0) {
          if (s->sound_widget) {
            lf_widget_t* active_widget = lf_widget_from_id(
              s->sound_widget->ui, 
              s->sound_widget->ui->root, 
              s->sound_widget->ui->active_widget_id);
            if (active_widget && active_widget->type == LF_WIDGET_TYPE_SLIDER) continue;
          }
          int percent = (int)(((vol - min) * 100) / (max - min));
          pthread_mutex_lock(&sound_mutex);
          sound_data.volume = percent;
          if (s->sound_widget) {
            lf_component_rerender(s->sound_widget->ui, soundwidget);
          }
          if(s->ui)
            lf_component_rerender(s->ui, uiutil);
          pthread_mutex_unlock(&sound_mutex);
        }
      }

      {
        // --- CAPTURE VOLUME (Mic) ---
        long min, max, vol;
        int pswitch;
        snd_mixer_selem_get_capture_volume_range(s->sndelem_capture, &min, &max);
        snd_mixer_selem_get_capture_volume(s->sndelem_capture, SND_MIXER_SCHN_FRONT_LEFT, &vol);
        snd_mixer_selem_get_capture_switch(s->sndelem_capture, SND_MIXER_SCHN_FRONT_LEFT, &pswitch);
        bool micmuted_before = sound_data.micmuted;
        sound_data.micmuted = pswitch == 0;
        if (sound_data.micmuted) {
          sound_data.microphone_before = sound_data.microphone;
          sound_data.microphone = 0;
        }
        if (micmuted_before != sound_data.micmuted) {
          pthread_mutex_lock(&sound_mutex);
          if (s->sound_widget) {
            lf_component_rerender(s->sound_widget->ui, soundwidget);
          }
          pthread_mutex_unlock(&sound_mutex);
        } else if (max - min > 0 && pswitch != 0) {
          if (s->sound_widget) {
            lf_widget_t* active_widget = lf_widget_from_id(
              s->sound_widget->ui, 
              s->sound_widget->ui->root, 
              s->sound_widget->ui->active_widget_id);
            if (active_widget && active_widget->type == LF_WIDGET_TYPE_SLIDER) continue;
          }
          int percent = (int)(((vol - min) * 100) / (max - min));
          pthread_mutex_lock(&sound_mutex);
          sound_data.microphone = percent;
          if (s->sound_widget) {
            lf_component_rerender(s->sound_widget->ui, soundwidget);
          }
          pthread_mutex_unlock(&sound_mutex);
        }
      }
    }
  }

  return NULL;
}


int main(void) {
  initxstate(&s);
  if(lf_windowing_init() != 0) {
    fail("cannot initialize libleif windowing backend.");
  }

  if(!alsasetup(&s)) return 1;
  pthread_t listener_thread;
  if (pthread_create(&listener_thread, NULL, alsalisten, (void *)&s) != 0) {
    fprintf(stderr, "boron: alsa: error creating listener thread\n");
    snd_mixer_close(s.sndhandle);
    return 1;
  }


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


  pthread_mutex_lock(&sound_mutex);

  s.pvstate = pv_init();
  if(!s.pvstate) return 1;
  s.sound_widget = pv_widget(
    s.pvstate, "boron_sound_popup", soundwidget,
    s.bararea.x + s.bararea.width - 300, 
    s.bararea.y + s.bararea.height + 10,
    300, 160);

  pthread_mutex_unlock(&sound_mutex);

  pv_widget_set_popup_of(s.pvstate, s.sound_widget, win);
  lf_widget_set_font_family(s.sound_widget->ui, s.sound_widget->ui->root, barfont);
  lf_widget_set_font_style(s.sound_widget->ui, s.sound_widget->ui->root, LF_FONT_STYLE_REGULAR);

  pv_widget_hide(s.sound_widget);
  pv_widget_set_animation(s.sound_widget, PV_WIDGET_ANIMATION_SLIDE_OUT_VERT, 0.2, lf_ease_out_cubic);
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
