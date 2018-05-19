#include <libnotify/notify.h>
#include <X11/Xlib.h>
#include <pulse/pulseaudio.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define LOWERVOL 122
#define INCREASEVOL 123
#define AUDIONAME "Audacious"
#define ICON "audio-speakers"

uint8_t run = 1;
pa_sink_input_info sinkinfo;
time_t last = 0;


Display *inithotkeys() {
	Display *display = XOpenDisplay(0);
	Window win = DefaultRootWindow(display);
	XGrabKey(display, LOWERVOL, 0, win, 0, GrabModeAsync, GrabModeAsync);
	XGrabKey(display, INCREASEVOL, 0, win, 0, GrabModeAsync, GrabModeAsync);
	XSelectInput(display, win, KeyPressMask);
	return display;
}

NotifyNotification *initnotif() {
	notify_init("musicbar");
	NotifyNotification *notif = notify_notification_new(AUDIONAME, NULL, ICON);
	notify_notification_set_timeout(notif, (gint)300);
	notify_notification_set_hint(notif, "synchronous", g_variant_new_string("volume"));
	return notif;
}

NotifyNotification *shownotif(NotifyNotification **notif, int8_t vol) { //Update and then show notification
	if(time(NULL) > last) {
		notify_notification_close(*notif, NULL);
		g_object_unref(G_OBJECT(*notif));
		notify_uninit();
		*notif = initnotif();
		puts("Killed notification");
	}
	
	last = time(NULL)+5;
	notify_notification_set_hint(*notif, "value", g_variant_new_int32(vol));
	notify_notification_show(*notif, NULL);
}

void parsesinks(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata) { //Callback to set sinkinfo
	if(i != NULL && strcmp(i->name, AUDIONAME) == 0) {
		memcpy(&sinkinfo, i, sizeof(pa_sink_input_info));
		pa_threaded_mainloop_signal( *(pa_threaded_mainloop **)userdata, 0);
	}
}

void volumesinkcb(pa_context *c, int success, void *userdata) { //callback for setting volume
	pa_threaded_mainloop_signal( *(pa_threaded_mainloop **)userdata, 0);
}

void pa_state_callback(pa_context *c, void *userdata) { //callback for getting context state
	*(pa_context_state_t *)userdata = pa_context_get_state(c);
}

uint8_t volumestuff(pa_context *context, pa_threaded_mainloop *mainloop, int16_t add) { //get volume from sink input, add to it, then set the new values
	pa_threaded_mainloop_lock(mainloop);
	pa_operation *op;
	
	op = pa_context_get_sink_input_info_list(context, parsesinks, &mainloop);
	while(pa_operation_get_state(op) == PA_OPERATION_RUNNING) pa_threaded_mainloop_wait(mainloop);
	pa_operation_unref(op);
	
	int32_t volume = add + sinkinfo.volume.values[0];
	if(volume > 65536) volume = 65536;
	if(volume < 0) volume = 0;
	
	for(uint8_t i = 0; i < sinkinfo.volume.channels; i++) {
		sinkinfo.volume.values[i] = volume;
	}
	
	op = pa_context_set_sink_input_volume(context, sinkinfo.index, &sinkinfo.volume, volumesinkcb, &mainloop);
	while(pa_operation_get_state(op) == PA_OPERATION_RUNNING) pa_threaded_mainloop_wait(mainloop);
	pa_operation_unref(op);
	
	pa_threaded_mainloop_unlock(mainloop);
	
	return (uint8_t)(volume / 655);
}

int main() {
	notify_init("musicbar");
	last = time(NULL);
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
		
	XEvent ev;
	while(run) {
		XNextEvent(display, &ev);
		if(ev.type == KeyPress) {
			shownotif(&notif, volumestuff(context, mainloop, ((ev.xkey.keycode == LOWERVOL) ? -655 : 655)) );
		}
	}
	return 0;
}