#!/bin/sh

GST_DEBUG=2 

if [ $# -ne 1 ]; then FILE=/apps/devnfs/test_videos/chroma_new/green02.flv; else FILE=$1; fi

CMD="gst-launch  \
\
filesrc location=$FILE ! \
queue ! \
flvdemux ! ffdec_flv ! queue ! identity sleep-time=10000 ! ffmpegcolorspace2 ! \
facetracker enableskin=true ! ffmpegcolorspace2 ! pyrlk ! \
ffmpegcolorspace2 ! timeoverlay! ffmpegcolorspace2 ! ximagesink  sync=false "


echo $CMD
$CMD
