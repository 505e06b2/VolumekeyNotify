#!/usr/bin/env python2
binary = "audacious"
from subprocess import call, Popen, PIPE
from re import search
from sys import argv
#import notify

try:
	addto = argv[1]
except IndexError:
	print "This script needs an argument (even if it's 0)"
	exit()

def done(sink, vol):
	call(["pactl", "set-sink-input-volume", sink, vol + "%"])
	call(["notify-send", "Music", "-i", "audio-speakers", "-h", "int:value:" + vol, "-h", "string:synchronous:volume",
		"-h", "string:x-canonical-private-synchronous:musicplayeraudio", "-t", "300"])

sinks = Popen(["pactl", "list", "sink-inputs"], stdout=PIPE)
volume = 100
for line in sinks.stdout.readlines():
	sinksearch = search("Sink Input #(.+)", line)
	if sinksearch: sink = sinksearch.group(1)

	volumesearch = search("/ (...)% /", line)
	if volumesearch: volume = volumesearch.group(1).lstrip()

	if search(binary, line):
		volume = int(addto) + int(volume)
		if volume > 100: volume = 100
		if volume < 0: volume = 0
		done(sink, str(volume))
		exit()