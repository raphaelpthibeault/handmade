#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

static Display *display;
static int screen;
static Window root;

static Window
CreateWindow(int x, int y, int w, int h, int b)
{
	Window win;
	XSetWindowAttributes xwa;

	xwa.background_pixel = WhitePixel(display, screen);
	xwa.border_pixel = BlackPixel(display, screen);
	xwa.event_mask = ExposureMask | Button1MotionMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask;

	win = XCreateWindow(display, root, x, y, w, h, b, DefaultDepth(display, screen), 
			InputOutput, DefaultVisual(display, screen), CWBackPixel | CWBorderPixel | CWEventMask, &xwa);

	return win;
}

static GC
CreateGC(int lineWidth)
{
	GC gc;
	XGCValues xgcv;
	unsigned long valueMask;

	xgcv.line_style = LineSolid;
	xgcv.line_width = lineWidth;
	xgcv.cap_style = CapButt;
	xgcv.join_style = JoinMiter;
	xgcv.fill_style = FillSolid;

	xgcv.foreground = BlackPixel(display, screen);
	xgcv.background = WhitePixel(display, screen);

	valueMask = GCForeground | GCBackground | GCFillStyle | GCJoinStyle | GCCapStyle | GCLineWidth | GCLineStyle;
	gc = XCreateGC(display, root, valueMask, &xgcv);

	return gc;
}

static bool Running;

int
main(int argc, char *argv[])
{
	Window mainWindow;
	XEvent event;
	GC gc;
	/* there's no graceful exit of an X11 event loop. There are a couple of ways to do it, I choose ClientMessage */
	Atom wmDeleteMessage;

	if ((display = XOpenDisplay(NULL)) == NULL)	
		err(EXIT_FAILURE, "Cannot open display");

	screen = DefaultScreen(display);
	root = RootWindow(display, screen);

	mainWindow = CreateWindow(50, 50, 1920, 1080, 15);
	XStoreName(display, mainWindow, "Handmade");
	XMapWindow(display, mainWindow);

	gc = CreateGC(4); // lineWidth=4

	wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, mainWindow, &wmDeleteMessage, 1);

	/* event loop */
	int init = 0, prevX = 0, prevY = 0;
	Running = true;
	while (Running)
	{
		XNextEvent(display, &event);
		switch (event.type)
		{
			case Expose:
				printf("Exposure\n");
				break;
			case ConfigureNotify: /* triggered when the window is resized, moved, or restacked */
				XConfigureEvent *xc = &event.xconfigure;
				printf("Resize or Move: New size is %dx%d\n", xc->width, xc->height);
				break;
			case ButtonPress:
				XDrawPoint(display, event.xbutton.window, gc, event.xbutton.x, event.xbutton.y);
				printf("Button Press\n");
				break;
			case MotionNotify:
				if (init)
				{
					XDrawLine(display, event.xbutton.window, gc, prevX, prevY, event.xbutton.x, event.xbutton.y);
				}
				else
				{
					XDrawPoint(display, event.xbutton.window, gc, event.xbutton.x, event.xbutton.y);
					init = 1;
				}
				prevX = event.xbutton.x;
				prevY = event.xbutton.y;
				break;
			case ButtonRelease:
				init = 0;
				printf("Button Release\n");
				break;
			case ClientMessage:
				if (event.xclient.data.l[0] == wmDeleteMessage)
				{
					printf("Window closed by user\n");
					Running = false;
				}
				break;
		}
	}

cleanup:
	XUnmapWindow(display, mainWindow);
	XFreeGC(display, gc);
	XDestroyWindow(display, mainWindow);
	XCloseDisplay(display);

	return 0; 
}
