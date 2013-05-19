#!/bin/sh


GST_DEBUG=2 

CMD="gst-launch v4l2src ! video/x-raw-yuv,width=320,height=240,framerate=25/1 ! ffmpegcolorspace2 ! facetracker  profile=\"./cascades/haar.txt\" enableskin=true display=false  ! ffmpegcolorspace2 ! draweventbox   ! ffmpegcolorspace2 ! ximagesink sync=false"

echo $CMD
$CMD
