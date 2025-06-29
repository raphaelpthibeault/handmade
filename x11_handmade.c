#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>


static Display *display;
static int screen;
static Window root;

int
main(int argc, char *argv[])
{
	Window win;
	XEvent event;

	if ((display = XOpenDisplay(NULL)) == NULL)	
		err(EXIT_FAILURE, "Cannot open display");

	screen = DefaultScreen(display);
	root = RootWindow(display, screen);

	win = XCreateSimpleWindow(display, root, 50, 50, 1920, 1080, 1, BlackPixel(display, 0), WhitePixel(display, 0));
	XMapWindow(display, win);


	XSelectInput(display, win, ExposureMask);

	/* event loop */
	while (true)
	{
		XNextEvent(display, &event);
		if (event.type == Expose)
		{
			XDrawString(display, win, DefaultGC(display, 0), 100, 100, "Hello, world", 12); 
		}

	}

	XUnmapWindow(display, win);
	XDestroyWindow(display, win);
	XCloseDisplay(display);

	return 0; 
}
