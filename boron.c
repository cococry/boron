#include <unistd.h>
#include <sys/wait.h>

#include <leif/color.h>
#include <leif/ez_api.h>
#include <leif/layout.h>
#include <leif/leif.h>

#include <leif/render.h>
#include <leif/timer.h>
#include <leif/ui_core.h>
#include <leif/util.h>
#include <leif/widget.h>
#include <leif/widgets/text.h>
#include <leif/win.h>
#include <runara/runara.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/xproto.h>

#include "config.h"

typedef enum {
  ewmh_atom_desktop_names = 0,
  ewmh_atom_current_desktop,
  ewmh_atom_count
} ewmh_atom_t;

typedef struct {
  int32_t x, y;
  uint32_t width, height;
} area_t;

typedef struct {
  xcb_connection_t* conn;
  xcb_screen_t* screen;
  lf_ui_state_t* ui;

  lf_div_t* barstart, *barcenter, *barend;

  xcb_atom_t ewmh_atoms[ewmh_atom_count];

  area_t bararea;
} state_t;

static state_t s;

static void initxstate(state_t* s);
static void printerror(const char* message);
static xcb_atom_t getatom(const char* atomstr, xcb_connection_t* conn);
static area_t getcursormon(xcb_connection_t* conn, xcb_screen_t* screen);
static area_t getbararea(xcb_connection_t* conn, xcb_screen_t* screen, area_t barmon);
static void setbarclasshints(xcb_window_t bar, xcb_connection_t* conn);
static void setwintypedock(xcb_window_t win, xcb_connection_t* conn, xcb_screen_t* screen);
static void setewmhstrut(xcb_window_t win, area_t area, xcb_connection_t* conn);

static char* getcmdoutput(const char* command);

void 
initxstate(state_t* s) {
  if(!s) return;
  
  s->conn = xcb_connect(NULL, NULL);
  if (xcb_connection_has_error(s->conn)) {
    printerror("Unable to open XCB connection");
  }

  const xcb_setup_t* setup = xcb_get_setup(s->conn);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  s->screen = iter.data;

  uint32_t values[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
  xcb_change_window_attributes(s->conn, s->screen->root, XCB_CW_EVENT_MASK, values);

  s->ewmh_atoms[ewmh_atom_desktop_names]    = getatom("_NET_DESKTOP_NAMES", s->conn);
  s->ewmh_atoms[ewmh_atom_current_desktop]  = getatom("_NET_CURRENT_DESKTOP", s->conn);
}

void 
printerror(const char* message) {
  fprintf(stderr, "boron: %s\n", message);
  exit(1);
}


xcb_atom_t
getatom(const char* atomstr, xcb_connection_t* conn) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, strlen(atomstr), atomstr);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}


area_t
getcursormon(xcb_connection_t* conn, xcb_screen_t* screen) {
  area_t monarea = {0, 0, 0, 0};

  // Query pointer
  xcb_query_pointer_cookie_t ptrcookie = xcb_query_pointer(conn, screen->root);
  xcb_query_pointer_reply_t* ptrreply = xcb_query_pointer_reply(conn, ptrcookie, NULL);

  if (!ptrreply) {
    printerror("unable to query pointer.\n");
    xcb_disconnect(conn);
    return monarea;
  }

  // Get screen resources
  xcb_randr_get_screen_resources_current_cookie_t rescookie = xcb_randr_get_screen_resources_current(conn, screen->root);
  xcb_randr_get_screen_resources_current_reply_t* resreply = xcb_randr_get_screen_resources_current_reply(conn, rescookie, NULL);

  if (!resreply) {
    printerror("unable to get screen resources\n");
    free(ptrreply);
    xcb_disconnect(conn);
    return monarea;
  }

  int noutputs = xcb_randr_get_screen_resources_current_outputs_length(resreply);
  xcb_randr_output_t* outputs = xcb_randr_get_screen_resources_current_outputs(resreply);

  for (int i = 0; i < noutputs; ++i) {
    xcb_randr_get_output_info_cookie_t outputinfocookie = xcb_randr_get_output_info(conn, outputs[i], XCB_CURRENT_TIME);
    xcb_randr_get_output_info_reply_t* outputinforeply = xcb_randr_get_output_info_reply(conn, outputinfocookie, NULL);

    if (!outputinforeply || outputinforeply->crtc == XCB_NONE) {
      free(outputinforeply);
      continue;
    }

    xcb_randr_get_crtc_info_cookie_t crtcinfocookie = xcb_randr_get_crtc_info(conn, outputinforeply->crtc, XCB_CURRENT_TIME);
    xcb_randr_get_crtc_info_reply_t* crtcinforeply = xcb_randr_get_crtc_info_reply(conn, crtcinfocookie, NULL);

    if (!crtcinforeply) {
      free(outputinforeply);
      continue;
    }

    if (ptrreply->root_x >= crtcinforeply->x && ptrreply->root_x < crtcinforeply->x + crtcinforeply->width &&
      ptrreply->root_y >= crtcinforeply->y && ptrreply->root_y < crtcinforeply->y + crtcinforeply->height) {
      monarea.x = crtcinforeply->x;
      monarea.y = crtcinforeply->y;
      monarea.width = crtcinforeply->width;
      monarea.height = crtcinforeply->height;
      free(crtcinforeply);
      free(outputinforeply);
      break;
    }

    free(crtcinforeply);
    free(outputinforeply);
  }

  free(resreply);
  free(ptrreply);

  return monarea;
}


area_t
getbararea(xcb_connection_t* conn, xcb_screen_t* screen, area_t barmon) {
  area_t bararea;

  switch(barmode) {
    case BarLeft: {
      bararea = (area_t){
        barmon.x + barmargin,
        barmon.y + barmargin,
        barsize, 
        barmon.height - (barmargin * 2)
      };
      break;
    }
    case BarRight: {
      bararea = (area_t){
        barmon.x + barmon.width - barsize - barmargin,
        barmon.y + barmargin,
        barsize, 
        barmon.height - (barmargin * 2)
      };
      break;
    }
    case BarTop: {
      bararea = (area_t){
        barmon.x + barmargin,
        barmon.y + barmargin,
        barmon.width - (barmargin * 2),
        barsize
      };
      break;
    }
    case BarBottom: {
      bararea = (area_t){
        barmon.x + barmargin,
        barmon.y + barmon.height - barsize -barmargin,
        barmon.width - (barmargin * 2),
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
setbarclasshints(xcb_window_t bar, xcb_connection_t* conn) {
  const char* resname = "boron";
  const char* resclass = "Boron";

  size_t len = strlen(resname) + strlen(resclass) + 2;  // +2 for the two null terminators

  char* class = (char*)malloc(len);
  if (!class) {
    return;
  }

  snprintf(class, len, "%s%c%s%c", resname, '\0', resclass, '\0');

  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, bar, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, len, class);

  free(class);
  xcb_flush(conn);
}

void 
setwintypedock(xcb_window_t win, xcb_connection_t* conn, xcb_screen_t* screen) {
  xcb_atom_t wintype_atom = getatom("_NET_WM_WINDOW_TYPE", conn);
  xcb_atom_t docktype_atom = getatom("_NET_WM_WINDOW_TYPE_DOCK", conn);
  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, wintype_atom, XCB_ATOM_ATOM, 32, 1, &docktype_atom);
  xcb_flush(conn);
}

void 
setewmhstrut(xcb_window_t win, area_t area, xcb_connection_t* conn) {
 xcb_atom_t strutpartial_atom = getatom("_NET_WM_STRUT_PARTIAL", conn);
  uint32_t strut[12] = {0};

  switch(barmode) {
    case BarLeft: {
      // Space reserved on the left
      strut[0] = area.width + barmargin * 2; 
      break;
    }
    case BarRight: {
      // Space reserved on the right 
      strut[1] = area.width + barmargin * 2; 
           break;
    }
    case BarTop: {
      // Space reserved on the top 
      strut[2] = area.height + barmargin * 2; 
      break;
    }
    case BarBottom: {
      // Space reserved on the bottom 
      strut[3] = area.height + barmargin * 2; 
      break;
    }
  }

  // Left start Y
  strut[4] = area.y;
  // Left end Y
  strut[5] = area.y + area.height - barmargin;
  // Right start Y
  strut[6] = area.y;
  // Right end Y
  strut[7] = area.y + area.height - barmargin; 
  // Top start X
  strut[8] = area.x;
  // Top end X
  strut[9] = area.x + area.width - barmargin; 
  // Top start X
  strut[10] = area.x;
  // Top end X
  strut[11] = area.x + area.width - barmargin;

  xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, strutpartial_atom, XCB_ATOM_CARDINAL, 32, 12, strut);
  xcb_flush(conn);
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

void updatebar(lf_ui_state_t* ui, lf_timer_t* updatetimer) {
  (void)updatetimer;
  uint32_t idxstart = 0, idxcenter = 0, idxend = 0; 
  for(uint32_t i = 0; i < (sizeof(cmds) / sizeof(displayed_command_t)); i++) {
    char* text = getcmdoutput(cmds[i].cmd);
    if(cmds[i].pos == BarPosStart) {
    lf_text_set_label(ui, (lf_text_t*)s.barstart->base.childs[idxstart++], text); 
    }
    if(cmds[i].pos == BarPosCenter) {
    lf_text_set_label(ui, (lf_text_t*)s.barcenter->base.childs[idxcenter++], text); 
    }
    if(cmds[i].pos == BarPosEnd) {
    lf_text_set_label(ui, (lf_text_t*)s.barend->base.childs[idxend++], text); 
    }
  }
    lf_ui_core_submit(ui);
}

int main(void) {

  if(lf_windowing_init() != 0) return EXIT_FAILURE;

  initxstate(&s);

  area_t barmon = getcursormon(s.conn, s.screen);

  s.bararea = getbararea(s.conn, s.screen, barmon);

  lf_ui_core_set_window_flags(LF_WINDOWING_X11_OVERRIDE_REDIRECT);
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_POS_X, s.bararea.x);
  lf_ui_core_set_window_hint(LF_WINDOWING_HINT_POS_Y, s.bararea.y);
  lf_window_t win = lf_ui_core_create_window(s.bararea.width, s.bararea.height, "Boron Bar");

  setbarclasshints((xcb_window_t)win, s.conn);
  setewmhstrut((xcb_window_t)win, s.bararea, s.conn);
  setwintypedock((xcb_window_t)win, s.conn, s.screen);

  s.ui = lf_ui_core_init(win);
 
  lf_widget_set_alignment(lf_crnt(s.ui), AlignCenterVertical);
  lf_ui_core_set_font(s.ui, "/usr/share/fonts/inter/InterNerdFontPropo-SemiBold.otf");

  lf_style_crnt_widget_prop(s.ui, color, lf_color_from_hex(0x171717));

  lf_div(s.ui);
  lf_style_crnt_widget_prop(s.ui, padding_left, 15.0f);
  lf_style_crnt_widget_prop(s.ui, padding_right, 15.0f);
  lf_widget_set_fixed_width_percent(lf_crnt(s.ui), 100.0f);

  lf_widget_set_layout(lf_crnt(s.ui), LayoutHorizontal);

  lf_crnt(s.ui)->justify_type = JustifySpaceBetween;

  s.barstart = lf_div(s.ui);
  lf_crnt(s.ui)->sizing_type = SizingFitToContent;
  lf_widget_set_layout(lf_crnt(s.ui), LayoutHorizontal);
  for(uint32_t i = 0; i < (sizeof(cmds) / sizeof(cmds[0])); i++) {
    if(cmds[i].pos != BarPosStart) continue;
    char* text = getcmdoutput(cmds[i].cmd);
    lf_text_h4(s.ui,text);
  }
  lf_div_end(s.ui);

  s.barcenter = lf_div(s.ui);
  lf_crnt(s.ui)->sizing_type = SizingFitToContent;
  lf_widget_set_layout(lf_crnt(s.ui), LayoutHorizontal);
  for(uint32_t i = 0; i < (sizeof(cmds) / sizeof(cmds[0])); i++) {
    if(cmds[i].pos != BarPosCenter) continue;
    char* text = getcmdoutput(cmds[i].cmd);
    lf_text_h4(s.ui,text);
  }
  lf_div_end(s.ui);

  s.barend = lf_div(s.ui);
  lf_crnt(s.ui)->sizing_type = SizingFitToContent;
  lf_widget_set_layout(lf_crnt(s.ui), LayoutHorizontal);
  for(uint32_t i = 0; i < (sizeof(cmds) / sizeof(cmds[0])); i++) {
    if(cmds[i].pos != BarPosEnd) continue;
    char* text = getcmdoutput(cmds[i].cmd);
    lf_text_h4(s.ui,text);
  }
  lf_div_end(s.ui);

  lf_ui_core_start_timer_looped(s.ui, 1.0f, updatebar);

  lf_div_end(s.ui);

  lf_widget_shape(s.ui, s.ui->root);
 
  while(s.ui->running) {
    lf_ui_core_next_event(s.ui);
  }
 
  lf_ui_core_terminate(s.ui);

  return EXIT_SUCCESS;
}

