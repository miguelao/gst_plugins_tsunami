#!/bin/sh

#interesting videos: looking left (/apps/devnfs/test_videos/chroma_new/green10.flv)
# and looking right (/apps/devnfs/test_videos/chroma_new/green03.flv)

if [ $# -ne 1 ]; then FILE=/apps/devnfs/test_videos/chroma_new/green02.flv; else FILE=$1; fi


CMD="gst-launch --gst-debug=facetracker:2 \
\
filesrc location=$FILE ! \
queue ! \
flvdemux ! ffdec_flv ! ffmpegcolorspace ! videoscale ! video/x-raw-rgb,width=320,height=240 ! \
queue max-size-buffers=1 ! identity sleep-time=50000 ! tee name=t0 \
t0. ! textoverlay text=\"original sequence\" halignment=0 valignment=2 shaded-background=true ! \
 ffmpegcolorspace2 ! vm.sink_1 \
\
t0. !  ffmpegcolorspace2 ! \
skin display=true ! ffmpegcolorspace2 ! \
textoverlay text=\"HSV\" halignment=0 valignment=2 shaded-background=true ! ffmpegcolorspace ! vm.sink_3 \
\
t0. !  ffmpegcolorspace2 ! \
skin display=true method=0 showh=true !  ffmpegcolorspace2 ! \
textoverlay text=\"H channel\" halignment=0 valignment=2 shaded-background=true ! \
ffmpegcolorspace2 ! vm.sink_2 \
\
t0. !  ffmpegcolorspace2 ! \
skin display=true method=0 shows=true !  ffmpegcolorspace2 ! \
textoverlay text=\"S channel\" halignment=0 valignment=2 shaded-background=true ! \
ffmpegcolorspace2 ! vm.sink_4 \
\\
t0. !  ffmpegcolorspace2 ! \
skin display=true method=0 showv=true !  ffmpegcolorspace2 ! \
textoverlay text=\"V channel\" halignment=0 valignment=2 shaded-background=true ! \
ffmpegcolorspace2 ! vm.sink_5 \
\
t0. !  ffmpegcolorspace2 ! \
skin display=true method=1 ! ffmpegcolorspace2 ! \
textoverlay text=\"RGB\" halignment=0 valignment=2 shaded-background=true ! ffmpegcolorspace ! vm.sink_6 \
\
 videotestsrc is-live=true pattern=2  ! video/x-raw-yuv, framerate=30/1, width=960, height=500 ! ffmpegcolorspace ! video/x-raw-rgb ! vm.sink_0 \
vmix name=vm \
sink_0::zorder=0 \

sink_1::zorder=1 sink_1::width=320 sink_1::height=240 sink_1::xpos=0   sink_1::ypos=0 \
sink_2::zorder=2 sink_2::width=320 sink_2::height=240 sink_2::xpos=320 sink_2::ypos=0 \
sink_3::zorder=3 sink_3::width=320 sink_3::height=240 sink_3::xpos=0   sink_3::ypos=240 \
sink_4::zorder=4 sink_4::width=320 sink_4::height=240 sink_4::xpos=320 sink_4::ypos=240 \
sink_5::zorder=5 sink_5::width=320 sink_5::height=240 sink_5::xpos=640 sink_5::ypos=0 \
sink_6::zorder=6 sink_6::width=320 sink_6::height=240 sink_6::xpos=640 sink_6::ypos=240 \
! ffmpegcolorspace ! ximagesink  sync=false"



echo $CMD
$CMD

