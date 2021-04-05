#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <csignal>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "cairo-xlib.h"
#include "cairo.h"

#include "use_gaze.h"

static cairo_surface_t *surface;
static cairo_t *cairo;
static Display *dpy;
static Window win;

static int width = 1920;
static int height = 1080;
static bool exited = false;

void cairo_init() {
    int screen;

    if ((dpy = XOpenDisplay(NULL)) == NULL) {
        std::cout << "Can't open display!";
        exit(1);
    }

    screen = DefaultScreen(dpy);
    win = XCreateSimpleWindow(
        dpy, DefaultRootWindow(dpy), 0, 0, width, height, 0, 0, 0);
    XSelectInput(dpy, win, ButtonPressMask | KeyPressMask);
    XMapWindow(dpy, win);

    surface = cairo_xlib_surface_create(
        dpy, win, DefaultVisual(dpy, screen), width, height);

    cairo = cairo_create(surface);
}

void paint(int x, int y, int size) {
    cairo_set_source_rgb(cairo, 255, 255, 255);
    cairo_rectangle(cairo, 0, 0, width, height);
    cairo_fill(cairo);

    cairo_set_line_width(cairo, 2);
    cairo_set_source_rgb(cairo, 255, 0, 0);
    cairo_rectangle(cairo, x - size * 0.5, y - size * 0.5, size, size);
    cairo_stroke(cairo);
}

void cairo_cleanup() {
    cairo_destroy(cairo);
    cairo_surface_destroy(surface);

    XUnmapWindow(dpy, win);
    XCloseDisplay(dpy);
}

void signal_handler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;

    exited = true;
}

int main(int argc, char *args[]) {
    cairo_init();

    signal(SIGINT, signal_handler);

    std::cout << "Initialized" << std::endl;

    use_gaze(
        [](const Point &p) {
            if (exited) {
                return true;
            }

            paint(p.x * width, p.y * height, 100);
            XFlush(dpy);

            return exited;
        },
        nullptr,
        nullptr,
        nullptr,
        true);

    std::cout << "Exit" << std::endl;

    cairo_cleanup();

    return 0;
};
