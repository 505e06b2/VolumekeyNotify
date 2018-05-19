#include <libnotify/notify.h>
#include <X11/Xlib.h>
#include <pulse/pulseaudio.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define LOWERVOL 122
#define INCREASEVOL 123

uint8_t run = 1;

NotifyNotification *initnotif() {
	notify_init("musicbar");
	NotifyNotification *n = notify_notification_new(NULL, NULL, "audio-speakers");
	notify_notification_set_hint(n, "synchronous", g_variant_new_string("volume"));
	notify_notification_set_hint(n, "id", g_variant_new_string("2002"));
	return n;
}

Display *inithotkeys() {
	Display *display = XOpenDisplay(0);
	Window win = DefaultRootWindow(display);
	XGrabKey(display, LOWERVOL, 0, win, 0, GrabModeAsync, GrabModeAsync);
	XGrabKey(display, INCREASEVOL, 0, win, 0, GrabModeAsync, GrabModeAsync);
	XSelectInput(display, win, KeyPressMask);
	return display;
}

void uninit(NotifyNotification *notif) {
	g_object_unref(G_OBJECT(notif));
	notify_uninit();
}

void changevol(int8_t vol) {
	
}

void shownotif(NotifyNotification *notif, int8_t vol) {
	notify_notification_set_hint(notif, "value", g_variant_new_int32(vol));
	notify_notification_show(notif, NULL);
}

pa_sink_input_info test;
void parsesinks(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) {
	if(i != NULL && strcmp(i->name, "Audacious") == 0) {
		memcpy(&test, i, sizeof(pa_sink_input_info));
		pa_threaded_mainloop_signal( *(pa_threaded_mainloop **)userdata, 0);
	}
}

void pa_state_callback(pa_context *c, void *userdata) {
	*(pa_context_state_t *)userdata = pa_context_get_state(c);
}

int main() {
	NotifyNotification *notif = initnotif();
	Display *display = inithotkeys();
	
	pa_threaded_mainloop *mainloop = pa_threaded_mainloop_new();
	pa_mainloop_api* mainloopapi = pa_threaded_mainloop_get_api(mainloop);
	pa_context *context = pa_context_new(mainloopapi, "Audacious Hotkeys");
	pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL);
	
	pa_context_state_t ready = 0;
	pa_context_set_state_callback(context, pa_state_callback, &ready);
	pa_threaded_mainloop_start(mainloop);
	puts("Started mainloop");
	while(ready != PA_CONTEXT_READY);
	puts("Connected");
	
	pa_threaded_mainloop_lock(mainloop);
	pa_operation *getsinkop = pa_context_get_sink_input_info_list(context, parsesinks, &mainloop);
	while(pa_operation_get_state(getsinkop) == PA_OPERATION_RUNNING) pa_threaded_mainloop_wait(mainloop);
	pa_operation_unref(getsinkop);
	pa_threaded_mainloop_unlock(mainloop);
	
	printf(">>> %u%\n", test.volume.values[0]/655);
		
	XEvent ev;
	while(run) {
		XNextEvent(display, &ev);
		if(ev.type == KeyPress) {
			int8_t vol = ((ev.xkey.keycode == LOWERVOL) ? -5 : 5);
			if(vol > 100) vol = 100;
			if(vol < 0) vol = 0;
			changevol(vol);
			shownotif(notif, vol);
		}
	}
	
	uninit(notif);
	return 0;
}