#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <stddef.h>
#include <stdint.h>

#include <math.h> // for sin in the animation

static Display *display;
static int screen;
static Window root;

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

static Window
CreateWindow(int x, int y, int width, int height, int b)
{
	Window win;
	XSetWindowAttributes xwa;

	xwa.background_pixel = WhitePixel(display, screen);
	xwa.border_pixel = BlackPixel(display, screen);
	xwa.event_mask = ExposureMask | StructureNotifyMask;

	win = XCreateWindow(display, root, x, y, width, height, b, DefaultDepth(display, screen), 
			InputOutput, DefaultVisual(display, screen), CWBackPixel | CWBorderPixel | CWEventMask, &xwa);

	return win;
}

static bool Running;

static Pixmap backbuffer = 0;
uint32_t *bitmapMemory = NULL;
int bitmapWidth, bitmapHeight;
static XImage *bitmapHandle = NULL;

static void
UpdateWindow(Window w, GC gc, int width, int height)
{
	XPutImage(display, backbuffer, gc, bitmapHandle, 0, 0, 0, 0, width, height);
	/* rectangle to rectangle copy */
	XCopyArea(display, backbuffer, w, gc, 0, 0, width, height, 0, 0);	
}

/* resize bitmap meant to represent Window w */
static void
ResizeBitmap(Window w, int width, int height)
{
	printf("Resize Bitmap with %dx%d\n", width, height);

	bitmapWidth = width;
	bitmapHeight = height;

	if (bitmapHandle)
	{
		/* NOTE: Also frees the bitmapMemory */
		XDestroyImage(bitmapHandle);
		bitmapHandle = NULL;
	}
	if (backbuffer)
	{
		XFreePixmap(display, backbuffer);
		backbuffer = 0;
	}

	bitmapMemory = malloc(sizeof(uint32_t) * width * height); // ARGB
	bitmapHandle = XCreateImage(display, DefaultVisual(display, screen), DefaultDepth(display, screen), ZPixmap, 0, (char *)bitmapMemory, width, height, 32, 0); // 32 bitmap pad?  0 bytes per line?
	backbuffer = XCreatePixmap(display, w, width, height, DefaultDepth(display, screen));
}


/* TODO Free everything at the end */

#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080

int
main(void)
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

	mainWindow = CreateWindow(1, 1, DEFAULT_WIDTH, DEFAULT_HEIGHT, 0);
	XStoreName(display, mainWindow, "Handmade");
	XMapWindow(display, mainWindow);

	gc = CreateGC(4); // lineWidth=4, why? idk

	ResizeBitmap(mainWindow, DEFAULT_WIDTH, DEFAULT_HEIGHT);

	wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, mainWindow, &wmDeleteMessage, 1);

	/* event loop */
	int prevWidth = DEFAULT_WIDTH, prevHeight = DEFAULT_HEIGHT;
	Running = true;
	double foo = 0.0;
	while (Running)
	{
		XNextEvent(display, &event);
		switch (event.type)
		{
			case Expose:
				//printf("Expose (force redraw)\n");
				/* Is this my problem? */
				break;
			case ConfigureNotify: /* triggered when window is resized, moved, or restacked */
				/* don't have to update the  mainWindow, that's done automatically */
				XConfigureEvent *xc = &event.xconfigure;
				if (xc->x < 0 || xc-> y < 0) 
				{
					/* not my problem */
					break;
				}

				printf("want window  %dx%d @ %dx%d\n", xc->width, xc->height, xc->x, xc->y);	
				if (prevWidth != xc->width || prevHeight != xc->height)
				{
					ResizeBitmap(mainWindow, xc->width, xc->height);
					prevWidth = xc->width;
					prevHeight = xc->height;
				}

				break;
			case ClientMessage:
				if (event.xclient.data.l[0] == wmDeleteMessage)
				{
					printf("Window closed by user\n");
					Running = false;
				}
				break;
		}

		/* some test render, idk */
		foo += 0.01;
		uint8_t red = (uint8_t)((sin(foo) * 0.5 + 0.5) * 255.0);
		uint8_t green = (uint8_t)((sin(foo + 2.0) * 0.5 + 0.5) * 255.0);
		uint8_t blue = (uint8_t)((sin(foo + 4.0) * 0.5 + 0.5) * 255.0);
		uint32_t color = (red << 16) | (green << 8) | blue;

		for (int i = 0; i < bitmapWidth * bitmapHeight; ++i)
		{
			bitmapMemory[i] = color;
		}

		UpdateWindow(mainWindow, gc, bitmapWidth, bitmapHeight);

	}

	if (bitmapHandle)
	{
		/* NOTE: Also frees the bitmapMemory */
		XDestroyImage(bitmapHandle);
		bitmapHandle = NULL;
	}
	if (backbuffer)
	{
		XFreePixmap(display, backbuffer);
		backbuffer = 0;
	}

	XUnmapWindow(display, mainWindow);
	XFreeGC(display, gc);
	XDestroyWindow(display, mainWindow);
	XCloseDisplay(display);

	return 0; 
}
