#!/bin/sh

#interesting videos: looking left (/apps/devnfs/test_videos/chroma_new/green10.flv)
# and looking right (/apps/devnfs/test_videos/chroma_new/green03.flv)

if [ $# -ne 1 ]; then FILE=/apps/devnfs/test_videos/chroma_new/green02.flv; else FILE=$1; fi

CMD="gst-launch --gst-debug=facedetector:2         \
filesrc location=$FILE ! \
flvdemux ! ffdec_flv ! identity sync=true ! ffmpegcolorspace2 ! \
facetracker3 display=true \
  profile=./cascades/haarcascade_frontalface_default.xml \
  profile2=./cascades/HS.xml    ! \
ffmpegcolorspace2 ! ximagesink sync=false"


echo $CMD
$CMD

