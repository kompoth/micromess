#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrandr.h>

#define OFFSET 2
#define ISIN(x,y,x0,y0,w,h) x>=x0 && x<=x0+w && y>=y0 && y<=y0+h

/* Micromess specific variables */
char *cmd;
int done;
int die_on_click;
int loc_hor, loc_ver;
char *mon_name;
int mx, my, mw, mh;

/* General variables */
int screen;
Display *dpy;
Window root, win;
Visual *vis;
Colormap cmap;
XftDraw *draw;
XftColor color;
XftFont *font;

void usage(FILE *output);
void finisher(int signal);
int init_x();
int get_monitor();
int init_xft(const char *fontdescr, const char *colordescr);
void spawn_window(const char *colordescr);
void redraw_window(const char *text, const int text_size);
void event_loop();

void
usage(FILE *output)
{
  fprintf(output, "usage: %s [-{b|t}|{l|r}|h|d] [-f font] [-F fgcolor] " \
                  "[-B bgcolor] [-m MONITOR]\n", cmd);
}

void
finisher(int signal) 
{
  done = 1;
  return;
}

int
init_x()
{
  if (!(dpy = XOpenDisplay(NULL)))
  {
    fprintf(stderr, "Failed to start X session.\n");
    return 1;
  }
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  vis = XDefaultVisual(dpy, screen);
  cmap = XDefaultColormap(dpy, screen);
  return 0;
}

int 
get_monitor()
{ 
  XRRMonitorInfo *mons;
  int nmons;
  Window winr, rootr; 
  int rx, ry, wx, wy;
  unsigned int mask;
  int i;
  int names_match;
  int pos_match;
  int ind = -1;

  /* Get monitors data */
  mons = XRRGetMonitors(dpy, root, 1, &nmons);
  if (nmons == -1) 
  {
    fprintf(stderr, "Failed to detect monitors.\n");
    return 1; 
  }
  
  /* Get monitor by name or by mouse pointer */
  XQueryPointer(dpy, root, &rootr, &winr, &rx, &ry, &wx, &wy, &mask);
  for (i = 0; i < nmons; i++) 
  {
    names_match = !strcmp(XGetAtomName(dpy, mons[i].name), mon_name);
    pos_match = ISIN(rx, ry, mons[i].x, mons[i].y, mons[i].width, mons[i].height);
    if (names_match || pos_match) 
    {
      ind = i;
      break;
    }  
  } 

  /* Write monitor position and dimensions */
  mx = mons[ind].x;
  my = mons[ind].y;
  mw = mons[ind].width;
  mh = mons[ind].height;
  return 0;
} 

int
init_xft(const char *fontdescr, const char *colordescr)
{
  font = XftFontOpenName(dpy, screen, fontdescr);
  if (!font)
  {
    fprintf(stderr, "Failed to loar font.\n");
    return 1; 
  }
  if (!XftColorAllocName(dpy, vis, cmap, colordescr, &color))
  {
    fprintf(stderr, "Failed to allocate color.\n");
    return 1;
  }
  draw = XftDrawCreate(dpy, win, vis, cmap);
  return 0;
}

void
spawn_window(const char *colordescr)
{
  XColor wincolor;
  XSetWindowAttributes swa;

  swa.override_redirect = True;  // unmanageable from wm
  swa.background_pixel = XWhitePixel(dpy, screen);
  win = XCreateWindow(dpy, root, 
                      0, 0, 1, 1, 1,  // geometry now doesn't matter 
                      CopyFromParent, CopyFromParent, CopyFromParent,
                      CWOverrideRedirect | CWBackPixel, &swa);
  XParseColor(dpy, cmap, colordescr, &wincolor);
  XAllocColor(dpy, cmap, &wincolor);
  XSetWindowBackground(dpy, win, wincolor.pixel);
  
  XSelectInput(dpy, win, ExposureMask|ButtonPressMask);
  XMapRaised(dpy, win);
  XFlush(dpy);
}

void 
redraw_window(const char *text, const int text_size) 
{
  XGlyphInfo text_info;
  int width, height, px, py;
  int xmargin, ymargin;

  XftTextExtentsUtf8(dpy, font, (const FcChar8 *)"\t", 1, &text_info);
  xmargin = text_info.width;
  ymargin = .5 * text_info.height;

  XftTextExtentsUtf8(dpy, font, (const FcChar8 *)text, text_size, &text_info);
  width = text_info.width + 2 * xmargin;
  height = text_info.height + 2 * ymargin;
  px = mx + .5 * loc_hor * (mw - width) + OFFSET * (1 - loc_hor);
  py = my + .5 * loc_ver * (mh - height) + OFFSET * (1 - loc_ver);
 
  XMoveWindow(dpy, win, px, py);
  XResizeWindow(dpy, win, width, height); 
  XftDrawStringUtf8(draw, &color, font, xmargin, height - ymargin, 
                    (const FcChar8 *)text, text_size);
  XFlush(dpy);
}

void
event_loop() 
{
  int redraw = 0;
  ssize_t length = 0;
  char input [1024] = {0,};
  XEvent ev;
  
  /* Prepare file descriptors for two types of events */
  nfds_t nfds = 2;
  struct pollfd fds[] = {
    {.fd = STDIN_FILENO,          .events = POLLIN},
    {.fd = ConnectionNumber(dpy), .events = POLLIN},
  };
   
  /* Start event loop */
  while (!done) 
  {
    if (poll(fds, nfds, -1) <= 0) continue;  // event catcher

    /* Input event */
    if (fds[0].revents & POLLHUP)  // eof!
      done = 1; 

    else if (fds[0].revents & POLLIN) // handle input
    {
      length = read(STDIN_FILENO, input, sizeof input);
      if (length < 0)  // this means something bad
      {
        fprintf(stderr, "Error while reading input data\n");
        break;
      }
      else if (length == 0)  // eof!
        done = 1;
      
      /* Display only the first line of the input string */
      char *eol =  memchr(input, '\n', length);
      if (eol)
      {
        *eol = '\0';
        length = (eol - input);
      }

      redraw = 1;
    }
    
    /* X11 event */
    while (fds[1].revents & POLLIN)
    {
      XNextEvent(dpy, &ev);
      
      if (ev.type == Expose && ev.xexpose.count == 0) 
        redraw = 1;
      
      if (!die_on_click)
        break;
      
      if (ev.type == ButtonPress && ev.xbutton.button == Button1)
      {
        done = 1;
        break;
      }
    }

    if (done) break;

    /* Update popup */
    if (redraw) {
      redraw_window(input, length);
      redraw = 0;
    }
  }
}

int
main(int argc, char **argv) 
{
  struct sigaction sa = {0};
  char ch;  // arg char
  done = 0;  // loop natural stopper
  char *fontname;
  char *fghex, *bghex;

  /* Parachute */
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = finisher;
  if (sigaction(SIGINT, &sa, NULL) || sigaction(SIGTERM, &sa, NULL))
  {
    fprintf(stderr, "Failed to set interuption handler.\n");
    return 1;
  }

  /* Parse args */
  cmd = argv[0];
  loc_hor = 1;  // left: 0,  center: 1,  right: 2
  loc_ver = 1;  //  top: 0,  center: 1, bottom: 2
  mon_name = "";
  die_on_click = 0;
  fontname = "monospace";
  fghex = "#000000";
  bghex = "#ffffff";
  while ((ch = getopt(argc, argv, "tblrf:F:B:m:dh")) != -1) 
  {
    switch (ch)
    {
      case 'h':
        usage(stdout);
        return 0;
      case 'd':
        die_on_click = 1;
        break;
      case 'l':
        loc_hor = 0;
        break;
      case 'r':
        loc_hor = 2;
        break;
      case 't':
        loc_ver = 0;
        break;
      case 'b':
        loc_ver = 2;
        break;
      case 'f':
        fontname = optarg; 
        break;
      case 'F':
        fghex = optarg; 
        break;
      case 'B':
        bghex = optarg; 
        break;
      case 'm':
        mon_name = optarg;
        break;
      default:
        usage(stderr);
        return 1;
    }
  }

  /* Prepare X, window, Xft */
  if (init_x())
    goto nowin;
  if (get_monitor())
    goto nowin;
  spawn_window(bghex);
  if (init_xft(fontname, fghex))
    goto nodraw;

  /* Handle events */
  event_loop();

  /* Clean */
  XftDrawDestroy(draw);
  XftColorFree(dpy, vis, cmap, &color);
nodraw:
  XDestroyWindow(dpy, win); 
nowin:
  XCloseDisplay(dpy);
  
  return !done;
}
