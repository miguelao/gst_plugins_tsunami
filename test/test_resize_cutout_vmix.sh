#!/bin/sh
 
gst-launch  --gst-debug=cutoutglsl:2,facedetect:2  \
\
filesrc location=$1 ! \
identity sync=true ! \
decodebin2 ! \
\
video/x-raw-yuv, framerate=25/1 ! \
videoscale ! \
video/x-raw-yuv, width=320, height=256, framerate=25/1 ! \
\
ffmpegcolorspace ! \
video/x-raw-rgb, framerate=25/1 ! \
cutoutglsl passthrough=false applycutout=true ! \
ffmpegcolorspace ! vm.sink_1 \
\
filesrc location=$2 ! \
identity sync=true ! \
decodebin2 ! \
\
video/x-raw-yuv, framerate=25/1 ! \
videoscale ! \
video/x-raw-yuv, width=320, height=256, framerate=25/1 ! \
\
ffmpegcolorspace ! \
video/x-raw-rgb, framerate=25/1 ! \
cutoutglsl passthrough=false applycutout=true ! \
ffmpegcolorspace ! vm.sink_2 \
\
filesrc location=$3 ! \
identity sync=true ! \
decodebin2 ! \
\
video/x-raw-yuv, framerate=25/1 ! \
videoscale ! \
video/x-raw-yuv, width=320, height=256, framerate=25/1 ! \
\
ffmpegcolorspace ! \
video/x-raw-rgb, framerate=25/1 ! \
cutoutglsl passthrough=false applycutout=true ! \
ffmpegcolorspace ! vm.sink_3 \
\
\
videotestsrc is-live=true pattern=2  ! video/x-raw-yuv, framerate=25/1, width=800, height=600 ! ffmpegcolorspace ! vm.sink_0 \
vmix name=vm \
sink_0::zorder=0 \
sink_1::zorder=1 sink_1::width=320 sink_1::height=240 sink_1::xpos=20    sink_1::ypos=300 \
sink_2::zorder=2 sink_2::width=320 sink_2::height=240 sink_2::xpos=250   sink_2::ypos=100 \
sink_3::zorder=3 sink_3::width=320 sink_3::height=240 sink_3::xpos=450   sink_3::ypos=300 \
 ! ffmpegcolorspace ! ximagesink sync=false
