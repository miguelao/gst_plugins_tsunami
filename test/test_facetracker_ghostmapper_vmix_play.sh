# First input to mixer defines outputsize + timebase for mixer output
# best to use videotestsrc w/ pattern=2 (black),zorder=0
# zorder, width, height, xpos/ypos can be adjusted dynamically while running

CMD="gst-launch --gst-debug=vmix:2,ghostmapper:4,decodebin2:4  \
videotestsrc is-live=true pattern=2  ! video/x-raw-yuv, framerate=30/1, width=640, height=480 ! \
timeoverlay valignment=1 ! ffmpegcolorspace ! vm.sink_0 \
\
filesrc location=\"$1\"  ! flvdemux name=deco0 ! ffdec_flv ! \
queue ! tee name=filein0 \
\
filein0. ! queue ! ffmpegcolorspace2 ! \
facetracker display=false  profile=./cascades/haar.txt enableskin=true ! \
ffmpegcolorspace2 name=between ! \
alpha ! ghostmapper passthrough=false  ghost=\"file:///apps/devnfs/test_videos/ghosts/mask_fg_bg3.png\"  ! \
ffmpegcolorspace2 name=after ! \
queue ! vm.sink_2 \
\
filein0. ! textoverlay text=\"facetracker 1 (blue-face detected, red-no face detected)\" halignment=0 valignment=2 shaded-background=true ! \
 ffmpegcolorspace2 ! \
 facetracker  display=true profile=\"./cascades/haar.txt\" enableskin=true ! ffmpegcolorspace2 ! \
vm.sink_1 \
\
vmix name=vm \
sink_0::zorder=0 \
sink_1::zorder=1 sink_1::width=320 sink_1::height=240 sink_1::xpos=0   sink_1::ypos=0 \
sink_2::zorder=2 sink_2::width=320 sink_2::height=240 sink_2::xpos=320 sink_2::ypos=0 \
sink_3::zorder=3 sink_3::width=320 sink_3::height=240 sink_3::xpos=0   sink_3::ypos=240 \
sink_4::zorder=4 sink_4::width=320 sink_4::height=240 sink_4::xpos=320 sink_4::ypos=240 \
! ffmpegcolorspace ! ximagesink sync=false \
\
deco0.audio ! queue ! fakesink sync=false "

echo $CMD
$CMD

################################################################################
# ALTERNATIVELY, substitute "facetracker..." with this line, for MCasas tracker (worse).
#facetracker3 display=true \
#  profile=./cascades/haarcascade_frontalface_default.xml \
#  profile2=./cascades/HS.xml    ! \