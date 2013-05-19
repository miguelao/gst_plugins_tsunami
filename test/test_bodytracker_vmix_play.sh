# First input to mixer defines outputsize + timebase for mixer output
# best to use videotestsrc w/ pattern=2 (black),zorder=0
# zorder, width, height, xpos/ypos can be adjusted dynamically while running

CMD="gst-launch --gst-debug=vmix:2,ghostmapper:4,decodebin2:4  \
videotestsrc is-live=true pattern=2  ! video/x-raw-yuv, framerate=30/1, width=640, height=480 ! \
timeoverlay valignment=1 ! ffmpegcolorspace ! vm.sink_0 \
\
filesrc location=\"$1\"  ! queue ! avidemux ! ffdec_indeo5  ! queue !
ffmpegcolorspace2 ! \
videoscale ! video/x-raw-rgb, width=320, height=240 ! ffmpegcolorspace2 ! \
facetracker3 display=true  profile=./cascades/haarcascade_fullbody.xml profile2=\" \" ! \
ffmpegcolorspace2 ! \
timeoverlay ! queue ! vm.sink_1 \
\
filesrc location=\"$2\"  ! queue ! avidemux ! ffdec_indeo5  ! queue ! \
ffmpegcolorspace2 ! \
videoscale ! video/x-raw-rgb, width=320, height=240 ! ffmpegcolorspace2 ! \
facetracker3 display=true  profile=./cascades/haarcascade_fullbody.xml profile2=\" \" ! \
ffmpegcolorspace2 ! \
timeoverlay ! queue ! vm.sink_2 \
\
filesrc location=\"$3\"  ! queue ! avidemux ! ffdec_indeo5  ! queue !
ffmpegcolorspace2 ! \
videoscale ! video/x-raw-rgb, width=320, height=240 ! ffmpegcolorspace2 ! \
facetracker3 display=true  profile=./cascades/haarcascade_fullbody.xml profile2=\" \" ! \
ffmpegcolorspace2 ! \
timeoverlay ! queue ! vm.sink_3 \
\
\
\
vmix name=vm \
sink_0::zorder=0 \
sink_1::zorder=1 sink_1::width=320 sink_1::height=240 sink_1::xpos=0   sink_1::ypos=0 \
sink_2::zorder=2 sink_2::width=320 sink_2::height=240 sink_2::xpos=320 sink_2::ypos=0 \
sink_3::zorder=3 sink_3::width=320 sink_3::height=240 sink_3::xpos=0   sink_3::ypos=240 \
sink_4::zorder=4 sink_4::width=320 sink_4::height=240 sink_4::xpos=320 sink_4::ypos=240 \
! ffmpegcolorspace ! ximagesink sync=false "

#! ffmpegcolorspace2 ! ffenc_flv bitrate=1500000 ! flvmux ! filesink location=./bodydetection01.flv"


echo $CMD
$CMD

