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

#define ISIN(x,y,x0,y0,w,h) x>=x0 && x<=x0+w && y>=y0 && y<=y0+h
#define XOFFSET 12 
#define YOFFSET 12

char *cmd;
int done;
int mx, my, mw, mh;  // focused monitor coords and dims
int loc_hor, loc_ver;
int screen;
Display *dpy;
Window root, win;
Visual *vis;
Colormap cmap;
XftDraw *draw;
XftColor color;
XftFont *font;


void
usage(FILE *output)
{
  fprintf(output, "usage: %s [-h|{b|t}|{l|r}] [-f font] [-F fgcolor] " \
                  "[-B bgcolor]\n", cmd);
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
get_focus(int *mx, int *my, int *mw, int *mh) 
{
  XRRMonitorInfo *mons;
  int nmons; 
  Window winr, rootr;  // win values, returned by xrandr
  int wx, wy, rx, ry;
  unsigned int mask;
  int i;
  int index = -1;
  
  // get monitors info
  mons = XRRGetMonitors(dpy, root, True, &nmons);
  if (nmons == -1) 
  {
    fprintf(stderr, "Failed to detect monitors.\n");
    return 1; 
  }
  
  // determine the focused window via mouse pointer
  if (XQueryPointer(dpy, root, &rootr, &winr, &rx, &ry, &wx, &wy, &mask))
  {
    // get focused monitor index
    for (i = 0; i < nmons; i++) 
    {
      if (ISIN(rx, ry, mons[i].x, mons[i].y, mons[i].width, mons[i].height)) 
      {
        index = i;
        break;
      }  
    } 
  }

  if (index < 0)
  {
    fprintf(stderr, "Failed to find active monitor.\n");
    return 1;
  }

  *mx = mons[index].x;
  *my = mons[index].y;
  *mw = mons[index].width;
  *mh = mons[index].height;
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
spawn_win(const char *colordescr)
{
  XColor wincolor;
  XSetWindowAttributes swa;

  swa.override_redirect = True;  // fixed position, unkillable from wm
  swa.background_pixel = XWhitePixel(dpy, screen);
  win = XCreateWindow(dpy, root, 
                      0, 0, 1, 1, 1,  // geometry now doesn't matter 
                      CopyFromParent, CopyFromParent, CopyFromParent,
                      CWOverrideRedirect | CWBackPixel, &swa);
  XParseColor(dpy, cmap, colordescr, &wincolor);
  XAllocColor(dpy, cmap, &wincolor);
  XSetWindowBackground(dpy, win, wincolor.pixel);
  
  XSelectInput(dpy, win, ExposureMask);
  XMapRaised(dpy, win);
  XFlush(dpy);
}

void 
redraw_win(const char *text, const int text_size) 
{
  XGlyphInfo ext;
  int width, height, px, py;


  XftTextExtentsUtf8(dpy, font, (const FcChar8 *)text, text_size, &ext);
  width = ext.width + 2 * XOFFSET;
  height = ext.height + 2 * YOFFSET;
  px = loc_hor * (mw - width) / 2;
  py = my + loc_ver * (mh - height) / 2;

  XMoveWindow(dpy, win, px, py);
  XResizeWindow(dpy, win, width, height); 
  XftDrawStringUtf8(draw, &color, font, XOFFSET, height - YOFFSET, 
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
    if (fds[1].revents & POLLIN)
    {
      XNextEvent(dpy, &ev);
      if (ev.type == Expose && ev.xexpose.count == 0) 
        redraw = 1;
    }
    
    /* Update popup */
    if (redraw) {
      redraw_win(input, length);
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
  fontname = "monospace";
  fghex = "#000000";
  bghex = "#ffffff";
  while ((ch = getopt(argc, argv, "htblrf:F:B:")) != -1) 
  {
    switch (ch)
    {
      case 'h':
        usage(stdout);
        return 0;
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
      default:
        usage(stderr);
        return 1;
    }
  }

  /* Prepare X, window, Xft */
  if (init_x())
    goto nowin;
  if (get_focus(&mx, &my, &mw, &mh))
    goto nowin;
  spawn_win(bghex);
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
