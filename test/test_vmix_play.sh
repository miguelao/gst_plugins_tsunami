# First input to mixer defines outputsize + timebase for mixer output
# best to use videotestsrc w/ pattern=2 (black),zorder=0
# zorder, width, height, xpos/ypos can be adjusted dynamically while running

# ximagesrc display-name=\":1\" screen-num=0 show-pointer=false use-damage=false ! video/x-raw-rgb, framerate=25/1 ! ffmpegcolorspace ! vm.sink_0 
# videotestsrc is-live=true pattern=2  ! video/x-raw-yuv, framerate=30/1, width=640, height=480 ! ffmpegcolorspace ! vm.sink_0
# multifilesrc location="bg_hills.jpg" ! jpegdec ! queue


gst-launch --gst-debug=vmix:3 \
multifilesrc location="$1" caps="image/jpeg,framerate=30/1" ! jpegdec !  ffmpegcolorspace ! vm.sink_0 \
videotestsrc is-live=true pattern=0  ! video/x-raw-yuv, framerate=30/1, width=320, height=240 ! ffmpegcolorspace ! vm.sink_1 \
videotestsrc is-live=true pattern=12 ! video/x-raw-yuv, framerate=30/1, width=320, height=240 ! ffmpegcolorspace ! vm.sink_2 \
videotestsrc is-live=true pattern=11 ! video/x-raw-yuv, framerate=30/1, width=320, height=240 ! ffmpegcolorspace ! vm.sink_3 \
videotestsrc is-live=true pattern=13 ! video/x-raw-yuv, framerate=30/1, width=320, height=240 ! ffmpegcolorspace ! vm.sink_4 \
vmix name=vm \
sink_0::zorder=0 \
sink_1::zorder=1 sink_1::width=320 sink_1::height=240 sink_1::xpos=0   sink_1::ypos=0 \
sink_2::zorder=2 sink_2::width=320 sink_2::height=240 sink_2::xpos=320 sink_2::ypos=0 \
sink_3::zorder=3 sink_3::width=320 sink_3::height=240 sink_3::xpos=0   sink_3::ypos=240 \
sink_4::zorder=4 sink_4::width=320 sink_4::height=240 sink_4::xpos=320 sink_4::ypos=240 \
! ffmpegcolorspace ! ximagesink  sync=false

