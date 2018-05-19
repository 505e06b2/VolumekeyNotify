#!/bin/sh
gcc notify.c `pkg-config --cflags --libs libnotify` -lX11 -lpulse -o notify
strip notify