#!/bin/sh
# this script launches a video-only FLV decode (user provides the video)
# and feeds a SSIM plugin with the original video and a modified version which is
# the result of flv-encoding+flv decoding of the same, at a certain bitrate, by default 60kbps

echo $#
if [ $# -ne 1 ]; then BITRATE=60000; else BITRATE=$1; fi

MEDIAPATH=/apps/devnfs/samplemedia
FILENAME=pirates.flv

BITRATE_TOLERANCE=`expr $BITRATE / 10`

CMD="gst-launch --gst-debug=ssim2:2  \
filesrc location=\"$MEDIAPATH/$FILENAME\" ! decodebin2 ! queue ! tee name=mux0 ! \
queue ! ffmpegcolorspace ! ssim0.sink_ref \
mux0. ! queue ! ffmpegcolorspace ! ffenc_flv bitrate=$BITRATE bitrate-tolerance=$BITRATE_TOLERANCE ! ffdec_flv ! ffmpegcolorspace ! ssim0.sink_test \
ssim2 name=ssim0 ! ffmpegcolorspace ! ximagesink sync=false  \
\
mux0. ! queue ! ffmpegcolorspace ! ximagesink sync=false"


echo $CMD
$CMD

#
#gst-launch --gst-debug=ssim2:4  \
#picturesrc location="file://`pwd`/test_picturesrc_data/sample001.jpg" ! video/x-raw-rgb, width=558, height=377, framerate=1/1 ! \
#ffmpegcolorspace ! ssim0.sink_ref \
#picturesrc location="file://`pwd`/test_picturesrc_data/sample001.jpg" ! video/x-raw-rgb, width=558, height=377, framerate=1/1 ! \
#ffmpegcolorspace ! gaussianblur ! \
#ffmpegcolorspace ! ssim0.sink_test \
#ssim2 name=ssim0 ! ffmpegcolorspace ! ximagesink sync=false
#
