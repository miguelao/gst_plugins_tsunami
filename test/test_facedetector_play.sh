#!/bin/sh

if [ $# -ne 1 ]; then FILE=/apps/devnfs/test_videos/chroma_new/green02.flv; else FILE=$1; fi

CMD="gst-launch --gst-debug=facedetector:2         \
filesrc location=$FILE ! \
decodebin2 ! identity sync=true ! ffmpegcolorspace2 ! \
facedetector drawbox=true classifier=\"./cascades/haarcascade_frontalface_default.xml\" ! \
ffmpegcolorspace2 ! ximagesink sync=false"


echo $CMD
$CMD












