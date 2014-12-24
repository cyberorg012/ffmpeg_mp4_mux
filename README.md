ffmpeg_mp4_mux
==============
use ffmpeg-2.4.4 to mux raw h264 data into mp4 format example

Compile:
mv ffmpeg-2.4.4/doc/examples/muxing.c ffmpeg-2.4.4/doc/examples/muxing.c_bak
cp muxing.c ffmpeg-2.4.4/doc/examples/muxing.c
cd ffmpeg-2.4.4/
make examples

Usage:
cd doc/examples
./muxing test.mp4
