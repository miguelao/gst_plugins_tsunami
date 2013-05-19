#!/bin/sh

GST_DEBUG=2 

if [ $# -lt 1 ]; then FILE=/apps/devnfs/test_videos/chroma_new/green02.flv; else FILE=$1; fi


CMD="gst-launch --gst-debug=contours:2 \
\
filesrc location=$FILE ! \
queue ! \
flvdemux ! ffdec_flv ! queue ! identity sleep-time=5000 ! \
ffmpegcolorspace ! videoscale ! video/x-raw-rgb, width=320, height=240 ! \
tee name=t0 \
t0. ! textoverlay text=\"original sequence\" halignment=0 valignment=2 shaded-background=true ! \
 ffmpegcolorspace2 ! vm.sink_1 \
\
\
t0. ! \
ffmpegcolorspace2 ! morphology iters=3 operator=2 !  ffmpegcolorspace2 ! \
textoverlay text=\"morph gradient\" halignment=0 valignment=2 shaded-background=true ! \
ffmpegcolorspace2 ! vm.sink_2 \
\
t0. ! \
ffmpegcolorspace2 ! morphology iters=3 operator=0 !  ffmpegcolorspace2 ! \
textoverlay text=\"opening\" halignment=0 valignment=2 shaded-background=true ! \
ffmpegcolorspace2 ! vm.sink_3 \
\
t0. ! \
ffmpegcolorspace2 ! morphology iters=3 operator=1 !  ffmpegcolorspace2 ! \
textoverlay text=\"closing\" halignment=0 valignment=2 shaded-background=true ! \
ffmpegcolorspace2 ! vm.sink_4 \
\
t0. ! \
ffmpegcolorspace2 ! morphology iters=3 operator=3 !  ffmpegcolorspace2 ! \
textoverlay text=\"top hat\" halignment=0 valignment=2 shaded-background=true ! \
ffmpegcolorspace2 ! vm.sink_5 \
\
t0. ! \
ffmpegcolorspace2 ! morphology iters=3 operator=4 !  ffmpegcolorspace2 ! \
textoverlay text=\"black hat\" halignment=0 valignment=2 shaded-background=true ! \
ffmpegcolorspace2 ! vm.sink_6 \
\
 videotestsrc is-live=true pattern=2  ! video/x-raw-yuv, framerate=30/1, width=960, height=500 ! ffmpegcolorspace ! video/x-raw-rgb ! vm.sink_0 \
vmix name=vm \
sink_0::zorder=0 \
sink_1::zorder=1 sink_1::width=320 sink_1::height=240 sink_1::xpos=0   sink_1::ypos=0 \
sink_2::zorder=2 sink_2::width=320 sink_2::height=240 sink_2::xpos=320 sink_2::ypos=0 \
sink_3::zorder=3 sink_3::width=320 sink_3::height=240 sink_3::xpos=0   sink_3::ypos=240 \
sink_4::zorder=4 sink_4::width=320 sink_4::height=240 sink_4::xpos=320 sink_4::ypos=240 \
sink_5::zorder=5 sink_5::width=320 sink_5::height=240 sink_5::xpos=640 sink_5::ypos=0   \
sink_6::zorder=6 sink_6::width=320 sink_6::height=240 sink_6::xpos=640 sink_6::ypos=240 \
! ffmpegcolorspace2 ! ffenc_flv bitrate=1500000 ! flvmux ! filesink location=./morphology$2.flv"

#! ffmpegcolorspace ! ximagesink  sync=false"


#CMD="gst-launch --gst-debug=pyrlk:2 filesrc location=/apps/devnfs/test_videos/chroma_new/green02.flv ! queue ! flvdemux ! ffdec_flv ! queue ! identity sleep-time=5000 ! ffmpegcolorspace2 ! video/x-raw-gray, width=320, height=240 ! contours display=true ! ffmpegcolorspace2 ! ximagesink sync=false"

echo $CMD
$CMD


#
