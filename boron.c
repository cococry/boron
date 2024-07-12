#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include <string.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>

#include <leif/leif.h>
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
  xcb_screen_t* screen;
  xcb_connection_t* conn;
  Display* dsp;
  LfState ui;

  xcb_window_t bar;
  GLXContext glcontext;

  ewmh_atom_t ewmh_atoms[ewmh_atom_count];
} state_t;

static void         printerror(const char* message);
static void         initxstate(state_t* s);
static xcb_window_t createwin(area_t geom, uint32_t bw, xcb_connection_t* conn, xcb_screen_t* screen);
static void         setbarclasshints(Display* dsp, xcb_window_t bar);

static GLXContext   createglcontext(Display* dsp);
static void         setglcontext(xcb_window_t win, Display* dsp, GLXContext context);
static void         eventloop(state_t* s);

static void         renderbar(xcb_window_t bar, LfState* ui, Display* dsp);
static LfState      initui(uint32_t winw, uint32_t winh);

static area_t       getcursormon(xcb_connection_t* conn, xcb_screen_t* screen); 
static char**       getdesktopnames(Display* dsp, uint32_t* o_numdesktops);
static int32_t      getcurrentdesktop(Display* dsp);
static xcb_atom_t   getatom(const char* atomstr, xcb_connection_t* conn);

static int32_t      compstrs(const void* a, const void* b);

void 
printerror(const char* message) {
  fprintf(stderr, "boron: %s\n", message);
  exit(1);
}

void
initxstate(state_t* s) {
  if(!s) return;
  s->dsp = XOpenDisplay(NULL);
  if(!s->dsp) {
    printerror("unable to open X display.");
  }

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

xcb_window_t 
createwin(area_t geom, uint32_t bw, xcb_connection_t* conn, xcb_screen_t* screen) {
  xcb_window_t win = xcb_generate_id(conn);
  uint32_t valmask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_OVERRIDE_REDIRECT;
  uint32_t vals[] = {
    screen->black_pixel,
    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS,
    1 
  };

  xcb_create_window(
    conn,
    XCB_COPY_FROM_PARENT,
    win,
    screen->root,
    geom.x, geom.y,
    geom.width, geom.height,
    bw,
    XCB_WINDOW_CLASS_INPUT_OUTPUT,
    screen->root_visual,
    valmask,vals 
  );

  xcb_map_window(conn, win);
  xcb_flush(conn);

  return win;
}

void
setbarclasshints(Display* dsp, xcb_window_t bar) {
  XClassHint classhint;
  classhint.res_name = "boron";
  classhint.res_class = "Boron";
  // Set the WM_CLASS property
  Atom wmclass = XInternAtom(dsp, "WM_CLASS", False);
  XSetClassHint(dsp, bar, &classhint);
}

GLXContext 
createglcontext(Display* dsp) {
  int screen = DefaultScreen(dsp);

  // Choose the appropriate visual
  int attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
  XVisualInfo* visual = glXChooseVisual(dsp, screen, attribs);
  if (!visual) {
    printerror("unable to choose visual.");
  }

  // Create OpenGL context
  GLXContext context = glXCreateContext(dsp, visual, NULL, GL_TRUE);
  if (!context) {
    printerror("unable to create GLX context.");
  }
  return context;
}

void
setglcontext(xcb_window_t win, Display* dsp, GLXContext context) {
  GLXDrawable drawable = (GLXDrawable)win;
  if (!glXMakeCurrent(dsp, drawable, context)) {
    printerror("unable to make GLX context current.");
  }
}

void
eventloop(state_t* s) {
  xcb_generic_event_t* event;
  while ((event = xcb_wait_for_event(s->conn))) {
    switch (event->response_type & ~0x80) {
      case XCB_EXPOSE:
        renderbar(s->bar, &s->ui, s->dsp);
        break;
      case XCB_PROPERTY_NOTIFY: {
        xcb_property_notify_event_t *property_notify = (xcb_property_notify_event_t *)event;
        if (property_notify->window == s->screen->root) {
          bool isinteresting = false;
          for(uint32_t i = 0; i < ewmh_atom_count; i++) {
            if(s->ewmh_atoms[i] == property_notify->atom) {
              isinteresting = true;
              break;
            }
          } 
          if(isinteresting) {
            renderbar(s->bar, &s->ui, s->dsp);
          }
        }
        break;
      }
    }
    free(event);
  }
}

void
renderbar(xcb_window_t bar, LfState* ui, Display* dsp) {
  {
    LfColor color = lf_color_from_hex(barcolor);
    vec4s zto = lf_color_to_zto(color);
    glClearColor(zto.r, zto.g, zto.b, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  lf_begin(ui);
  uint32_t numdesktops;
  char** desktops = getdesktopnames(dsp, &numdesktops);
  
  int32_t currentdesktop = getcurrentdesktop(dsp);

  for(uint32_t i = 0; i < numdesktops; i++) {
    bool isfocused = currentdesktop == i;
    LfUIElementProps props = ui->theme.button_props;
    props.padding = 0;
    props.margin_top = 0;
    props.margin_left = 0;
    props.margin_right = 0;
    props.color = isfocused ? lf_color_from_hex(barcolor_desktop_focused) : LF_NO_COLOR;
    props.text_color =  isfocused ? lf_color_from_hex(barcolor_desktop_focused_font) : lf_color_from_hex(bartextcolor);
    props.border_width = 0;
    lf_push_style_props(ui, props);
    lf_button_fixed(ui, desktops[i], barheight, barheight);
    lf_pop_style_props(ui);
  }
  for(uint32_t i = 0; i < numdesktops; i++) {
    free(desktops[i]);
  }
  lf_end(ui);
  glXSwapBuffers(dsp, bar);
}

LfState
initui(uint32_t winw, uint32_t winh) {
  LfState ui = lf_init_x11(winw, winh);
  lf_free_font(&ui.theme.font);
  ui.theme.font = lf_load_font(barfontpath, barfontsize);
  ui.theme.text_props.text_color = lf_color_from_hex(bartextcolor);
  ui.theme.scrollbar_width = 0;
  return ui;
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

char** getdesktopnames(Display* dsp, uint32_t* o_numdesktops) {
    Atom netdesktopnames;
    Atom type;
    int format;
    unsigned long nitems, bytes;
    unsigned char *data = NULL;

    // Get the Atom for _NET_DESKTOP_NAMES
    netdesktopnames = XInternAtom(dsp, "_NET_DESKTOP_NAMES", False);

    // Get the property value from the root window
    if (XGetWindowProperty(dsp, DefaultRootWindow(dsp), netdesktopnames,
                           0, 0x7FFFFFFF, False, XA_STRING, &type, &format,
                           &nitems, &bytes, &data) != Success) {
        printerror("Failed to get _NET_DESKTOP_NAMES property.\n");
        return NULL;
    }

    // Check if the atom type is what we expect
    if (type != XA_STRING || format != 8) {
        printerror("_NET_DESKTOP_NAMES property has unexpected type or format.\n");
        XFree(data);
        return NULL;
    }

    // Count the number of desktop names
    char *data_ptr = (char *)data;
    int count = 0;
    for (char *p = data_ptr; p < data_ptr + nitems; ++p) {
        if (*p == '\0') {
            ++count;
        }
    }

    // Allocate memory for the array of desktop names
    char **names = (char **)malloc(count * sizeof(char *));
    if (names == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        XFree(data);
        return NULL;
    }

    // Copy the desktop names into the array
    int index = 0;
    char *name_start = data_ptr;
    for (char *p = data_ptr; p < data_ptr + nitems; ++p) {
        if (*p == '\0') {
            names[index] = strndup(name_start, p - name_start);
            if (names[index] == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
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

    // Free the property data and set the number of desktops
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
  unsigned char* prop;

  // Atom for the _NET_CURRENT_DESKTOP property
  netcurrentdesktop = XInternAtom(dsp, "_NET_CURRENT_DESKTOP", False);

  // Get the property from the root window
  if (XGetWindowProperty(dsp, DefaultRootWindow(dsp), netcurrentdesktop, 0, 1, False, XA_CARDINAL, 
                         &type, &format, &nitems, &bytes, &prop) != Success || type != XA_CARDINAL || nitems != 1) {
    fprintf(stderr, "Failed to get the _NET_CURRENT_DESKTOP property.\n");
    return -1; // Indicate an error
  }

  // Convert the property value to an integer
  int currentdesktop = *(long*)prop;

  // Free the memory allocated for the property
  XFree(prop);

  return currentdesktop;
}

xcb_atom_t
getatom(const char* atomstr, xcb_connection_t* conn) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, strlen(atomstr), atomstr);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}

int32_t 
compstrs(const void* a, const void* b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

int
main() {
  state_t s;
  initxstate(&s);

  area_t barmon = getcursormon(s.conn, s.screen);
  area_t bararea = (area_t){
    barmon.x + barmargin,
    barmon.y + barmargin,
    barmon.width - (barmargin * 2),
    barheight
  };
  s.bar = createwin(bararea, 
                    barborderwidth,
                    s.conn, s.screen);

  setbarclasshints(s.dsp, s.bar);

  s.glcontext = createglcontext(s.dsp);
  setglcontext(s.bar, s.dsp, s.glcontext);

  s.ui = initui(bararea.width, bararea.height);

  renderbar(s.bar, &s.ui, s.dsp);

  // Main loop
  eventloop(&s);

  return 0;
}

