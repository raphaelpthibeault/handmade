#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <stddef.h>
#include <stdint.h>

static Display *display;
static int screen;
static Window root;

static bool Running;
static int BytesPerPixel = 4;

static Pixmap backbuffer = 0;
static int bitmapWidth, bitmapHeight;
static void *bitmapMemory = NULL;
static XImage *bitmapHandle = NULL;
static Atom closeAtom; /* there's no graceful exit of an X11 event loop. There are a couple of ways to do it, I choose ClientMessage */

static XVisualInfo xvi;
static XShmSegmentInfo shmInfo;

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

	xwa.bit_gravity = StaticGravity; // stops most of the flicker when resizing; TODO: need a better fix

	xwa.background_pixel = WhitePixel(display, screen);
	xwa.border_pixel = BlackPixel(display, screen);
	xwa.event_mask = ExposureMask | StructureNotifyMask;

	win = XCreateWindow(display, root, x, y, width, height, b, DefaultDepth(display, screen), 
			InputOutput, DefaultVisual(display, screen), CWBackPixel | CWBorderPixel | CWEventMask, &xwa);

	return win;
}

static void
UpdateWindow(Window w, GC gc, int width, int height)
{
	XShmPutImage(display, backbuffer, gc, bitmapHandle, 0, 0, 0, 0, width, height, False);
	/* rectangle to rectangle copy */
	XCopyArea(display, backbuffer, w, gc, 0, 0, width, height, 0, 0);	
}

static void
RenderWeirdGradient(int xOffset, int yOffset)
{
	int pitch = bitmapWidth * BytesPerPixel; // difference between a row and the next row (which is width * bytesPerPixel)
	uint8_t *row = bitmapMemory;
	for (int y = 0; y < bitmapHeight; ++y)
	{
		uint32_t *pixel = (uint32_t *)row;
		for (int x = 0; x < bitmapWidth; ++x)
		{
			/* little endian,
			 * memory:	0xAABBGGRR 
			 * register:0xAARRGGBB  (because OS developers didn't want to look at RGB backwards in register)
			 * */
			uint8_t r = 0;
			uint8_t b = x + xOffset;
			uint8_t g = y + yOffset;
			*pixel++ = (r << 16) | (g << 8) | b ;
		}

		row += pitch;
	}
}


/* resize bitmap meant to represent Window w */
static void
ResizeBitmap(Window w, int width, int height)
{
	printf("Resize Bitmap with %dx%d\n", width, height);

	if (bitmapHandle)
	{
		XShmDetach(display, &shmInfo); 
		XSync(display, False); /* X11 documentation is shit (or am I suffering from skill issues?), 
														* I can't see anything more direct to free the shared memory so I call XSync() here to trigger
		                        * the "shmctl()" lower down in the function (that was called previously if the bitmapHandle exists)
														* But this causes a race condition (shmdt is sequential/serial, it runs immediately)
														* Must call XSync() before shmdt() to be sure the server has detached from the old segment 
														*/
		shmdt(shmInfo.shmaddr);

		/* NOTE: Also frees the bitmapMemory */
		XDestroyImage(bitmapHandle);
		bitmapHandle = NULL;
	}
	if (backbuffer)
	{
		XFreePixmap(display, backbuffer);
		backbuffer = 0;
	}

	bitmapWidth = width;
	bitmapHeight = height;

	/* shared memory extension */
	bitmapHandle = XShmCreateImage(
			display,
			xvi.visual,
			xvi.depth,
			ZPixmap,
			NULL,
			&shmInfo,
			bitmapWidth, bitmapHeight);

	if (!bitmapHandle)
		err(EXIT_FAILURE, "XShmCreateImage() failure");

	shmInfo.shmid = shmget(
			IPC_PRIVATE,
			bitmapHandle->bytes_per_line * bitmapHandle->height,
			IPC_CREAT | 0600 // read and write perms
			);

	if (shmInfo.shmid < 0)
		err(EXIT_FAILURE, "shmget() failure");

	shmInfo.shmaddr = shmat(shmInfo.shmid, 0, 0);
	if (shmInfo.shmaddr == (void *)-1) 
		err(EXIT_FAILURE, "shmat failed");

	bitmapMemory = bitmapHandle->data = shmInfo.shmaddr;

	shmInfo.readOnly = False;
	XShmAttach(display, &shmInfo);

	/* mark segment for destruction 
	 * "The segment will actually be destroyed only after the last process detaches it (i.e., when the shm_nattch member of the associated structure shmid_ds is zero)."
	 * i.e. kernel will destroy once both X server and process detach (call XSmhDetach)
	 * */
	shmctl(shmInfo.shmid, IPC_RMID, NULL);

	backbuffer = XCreatePixmap(display, w, bitmapWidth, bitmapHeight, xvi.depth);
}

/* default to 1080p */
#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080

int
main(void)
{
	Window mainWindow;
	XEvent event;
	GC gc;

	if ((display = XOpenDisplay(NULL)) == NULL)	
		err(EXIT_FAILURE, "Cannot open display");

	if (!XShmQueryExtension(display))
		err(EXIT_FAILURE, "No Shm extension");

	screen = DefaultScreen(display);

	if (!XMatchVisualInfo(display, screen, DefaultDepth(display, screen), TrueColor, &xvi))
		err(EXIT_FAILURE, "MatchVisualInfo() error");

	root = RootWindow(display, screen);
	mainWindow = CreateWindow(1, 1, DEFAULT_WIDTH, DEFAULT_HEIGHT, 0);
	XStoreName(display, mainWindow, "Handmade");
	XMapWindow(display, mainWindow);

	gc = CreateGC(4); // lineWidth=4, why? idk

	/* creates image, pixmap, etc */
	ResizeBitmap(mainWindow, DEFAULT_WIDTH, DEFAULT_HEIGHT);

	closeAtom = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, mainWindow, &closeAtom, 1);

	/* event loop */
	int prevWidth = DEFAULT_WIDTH, prevHeight = DEFAULT_HEIGHT;
	Running = true;
	int xOffset = 0, yOffset = 0;
	while (Running)
	{
		/* Process all events */
		while (XPending(display))
		{
			XNextEvent(display, &event); // blocks
			switch (event.type)
			{
				case Expose:
				{
					UpdateWindow(mainWindow, gc, bitmapWidth, bitmapHeight);
					break;
				}
				case ConfigureNotify: /* triggered when window is resized, moved, or restacked */
				{
					/* don't have to update the mainWindow, that's done automatically */
					XConfigureEvent *xc;
					xc = &event.xconfigure;
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
				}
				case ClientMessage:
				{
					if ((Atom)event.xclient.data.l[0] == closeAtom)
					{
						printf("Window closed by user\n");
						Running = false;
					}
					break;
				}
			}
		}

		/* Render */
		RenderWeirdGradient(xOffset, yOffset);
		UpdateWindow(mainWindow, gc, bitmapWidth, bitmapHeight);
		XFlush(display);

		++xOffset;
	}

	if (bitmapHandle)
	{
		XShmDetach(display, &shmInfo); 
		shmdt(shmInfo.shmaddr);

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
