#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/Xrandr.h>

#include <GL/gl.h>
#include <GL/glx.h>

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

typedef struct {
    int32_t x, y;
    uint32_t width, height;
} area_t;

typedef struct {
  Display* dsp;
  Window bar;
  GC gc;
  LfState ui;

  area_t barmon;
  area_t bargeom;

  uint32_t numdesktops;
} state_t;


static state_t s;

static void     createbar();
static void     renderbar();
static void     terminate();

static area_t   cursormon();
static uint32_t getnumdesktops();

void
createbar() {
  // Get true-color visual
  XVisualInfo visualinfo;
  XMatchVisualInfo(s.dsp, DefaultScreen(s.dsp), 32, TrueColor, &visualinfo);

  // Setup window attributes and create window
  XSetWindowAttributes attr;
  attr.colormap   = XCreateColormap(s.dsp, DefaultRootWindow(s.dsp), visualinfo.visual, AllocNone);
  attr.event_mask = ExposureMask | KeyPressMask;
  attr.background_pixmap = None;
  attr.border_pixel      = 0;
  attr.override_redirect = True;

  s.barmon = cursormon();

  s.bargeom = (area_t){
    .x = s.barmon.x + barmargin,
    .y = s.barmon.y + barmargin,
    .width = s.barmon.width - (barmargin * 2),
    .height = barheight
  };

  s.bar = XCreateWindow(s.dsp, 
                      DefaultRootWindow(s.dsp),
                      s.bargeom.x, s.bargeom.y,
                      s.bargeom.width, s.bargeom.height,
                      0,
                      visualinfo.depth,
                      InputOutput,
                      visualinfo.visual,
                      CWColormap|CWEventMask|CWBackPixmap|CWBorderPixel|CWOverrideRedirect,
                      &attr
                      );

  // Create X Graphics Context
  s.gc = XCreateGC(s.dsp, s.bar, 0, 0);

  // Set window name
  XStoreName(s.dsp, s.bar, "boron");

  XClassHint* classhint = XAllocClassHint();
  if (!classhint) {
    fprintf(stderr, "boron: cannot allocate memory for XClassHint\n");
    exit(1);
  }
  classhint->res_name = "boron";
  classhint->res_class = "Boron";

  XSetClassHint(s.dsp, s.bar, classhint);

  // Create OpenGL context
  GLXContext glcontext = glXCreateContext(s.dsp, &visualinfo, 0, True );
  if (!glcontext)
  {
    fprintf(stderr, "boron: failed to create GLX context.");
    exit(1);
  }
  glXMakeCurrent(s.dsp, s.bar, glcontext);

  // Init UI
  s.ui = lf_init_x11(s.bargeom.width, s.bargeom.height);
  lf_free_font(&s.ui.theme.font);
  s.ui.theme.font = lf_load_font(barfontpath, barfontsize);
  s.ui.theme.text_props.text_color = bartextcolor;

  glViewport(0, 0, s.bargeom.width, s.bargeom.height);
  lf_resize_display(&s.ui, s.bargeom.width, s.bargeom.height);

  // Make the window visible by mapping it 
  XMapWindow(s.dsp, s.bar);
}
void
renderbar() {
  {
    if(bartransparent) {
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    } else {
      LfColor color = lf_color_from_hex(barcolor);
      vec4s zto = lf_color_to_zto(color);
      glClearColor(zto.r, zto.g, zto.b, zto.a);
    }
    glClear(GL_COLOR_BUFFER_BIT);
  }

  lf_begin(&s.ui);
  for(uint32_t i = 0; i < s.numdesktops; i++) {
    char idx[2];
    sprintf(idx, "%i", i + 1);
    LfUIElementProps props = s.ui.theme.text_props;
    props.margin_top = (barheight - lf_text_dimension(&s.ui, idx).y) / 2.0f;
    props.margin_left = 7.5f;
    lf_push_style_props(&s.ui, props);
    lf_text(&s.ui, idx);
    lf_pop_style_props(&s.ui);
  }
  lf_end(&s.ui);

  glXSwapBuffers(s.dsp, s.bar);
  glXWaitGL();
}

void
terminate() {
  XDestroyWindow(s.dsp, s.bar);
  s.bar = 0;
  XCloseDisplay(s.dsp); 
  s.dsp = NULL;
}

area_t
cursormon() {
  area_t result = {0, 0, 0, 0};

  // Get the root window
  Window root = DefaultRootWindow(s.dsp);

  // Query pointer
  Window rootret, childret;
  int rootx, rooty;
  int winx, winy;
  unsigned int maskret;

  if (XQueryPointer(s.dsp, root, &rootret, &childret, &rootx, &rooty, &winx, &winy, &maskret)) {
    // Get the XRandR screen resources
    XRRScreenResources* screenres = XRRGetScreenResources(s.dsp, root);
    if (screenres) {
      // Iterate over each output
      for (int i = 0; i < screenres->noutput; i++) {
        XRROutputInfo* outputinfo = XRRGetOutputInfo(s.dsp, screenres, screenres->outputs[i]);
        if (outputinfo && outputinfo->crtc) {
          // Get CRTC info (this contains the position and size of the monitor)
          XRRCrtcInfo* crtcinfo = XRRGetCrtcInfo(s.dsp, screenres, outputinfo->crtc);
          if (crtcinfo) {
            // Check if the cursor is within this monitor
            if (rootx >= crtcinfo->x && rootx < crtcinfo->x + crtcinfo->width &&
              rooty >= crtcinfo->y && rooty < crtcinfo->y + crtcinfo->height) {
              result.x = crtcinfo->x;
              result.y = crtcinfo->y;
              result.width = crtcinfo->width;
              result.height = crtcinfo->height;
              XRRFreeCrtcInfo(crtcinfo);
              break;
            }
            XRRFreeCrtcInfo(crtcinfo);
          }
        }
        if (outputinfo) {
          XRRFreeOutputInfo(outputinfo);
        }
      }
      XRRFreeScreenResources(screenres);
    }
  }

  return result;
}

uint32_t
getnumdesktops() {
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char* prop = NULL;
  Atom netnumberofdesktops = XInternAtom(s.dsp, "_NET_NUMBER_OF_DESKTOPS", True);

  if (netnumberofdesktops == None) {
    fprintf(stderr, "boron: XInternAtom failed for _NET_NUMBER_OF_DESKTOPS.\n");
    return 0;
  }

  if (XGetWindowProperty(s.dsp, DefaultRootWindow(s.dsp), netnumberofdesktops,
                         0, 1, False, XA_CARDINAL, &type, &format,
                         &nitems, &bytesafter, &prop) != Success) {
    fprintf(stderr, "boron: XGetWindowProperty failed.\n");
    return 0;
  }

  if (type == XA_CARDINAL && format == 32 && nitems == 1) {
    uint32_t numdesktops = *(unsigned long *)prop;
    XFree(prop);
    return numdesktops;
  }

  if (prop) {
    XFree(prop);
  }
  return 0;
}

int 
main(int argc, char* argv[]) {
  s.dsp = XOpenDisplay(NULL);

  if (!s.dsp) {
    fprintf(stderr, "ragnar: cannot open X display"); 
    exit(1);
  }

  createbar();

  s.numdesktops = getnumdesktops();

  while(1) {
    while(XPending(s.dsp) > 0) {
      // process event
      XEvent event;
      XNextEvent(s.dsp, &event);

      switch(event.type) {  
        case Expose:
          renderbar();
          break;
        default:
          break;
      }
    }
  }

  terminate();

  return 0;
}
