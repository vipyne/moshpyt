# moshpyt

this is the early beginnings of a CLI to [datamosh](http://datamoshing.com/) videos.  heavily under construction.

something, something ffmpeg

![](https://github.com/vipyne/moshpyt/blob/master/moshpyt_readme.gif)

#### Usage (works 12% of the time)
- need better timecode handling probably

```sh
$ make mosh

$ moshpyt motion_vectors.mov image.mov output_video.mov
```

ffmpeg stuff needed:

`-L/usr/local/Cellar/ffmpeg/3.1.5/lib`  # your path to ffmpeg 3.1.5 / 3.4.1 / 4.1

with these libs:
- `lavdevice`
- `lavformat`
- `lavfilter`
- `lavcodec`
- `lswresample`
- `lswscale`
- `lavutil`

----------

```sh
make mosh2 && ./moshthese2 ~/Movies/dance.mp4 ~/Movies/silliness.mp4 out$(date +%s).mp4
```

```sh
$ gcc moshpyt.c \
-L/usr/local/Cellar/ffmpeg/3.1.5/lib \
-lavdevice \
-lavformat \
-lavfilter \
-lavcodec \
-lswresample \
-lswscale \
-lavutil \
-o moshthese

$ ./moshthese ~/Movies/bobble_vector.mov ~/Movies/image.mov ~/desktop/moshpyt$(date +%s).mp4
```

notes to self

make mosh2 && ./moshthese2 ~/Movies/dance.mp4 ~/Movies/silliness.mp4 a_b_$(date +%s).mp4
https://stackoverflow.com/questions/50992787/ffmpeg-what-does-av-parser-parse2-do
https://ffmpeg.org/doxygen/trunk/decoding__encoding_8c-source.html

NOTE1: some codecs are stream based (mpegvideo, mpegaudio)
and this is the only method to use them because you cannot
know the compressed data size before analysing it.

BUT some other codecs (msmpeg4, mpeg4) are inherently frame
based, so you must call them with all the data for one
frame exactly. You must also initialize 'width' and
'height' before initializing them.
NOTE2: some codecs allow the raw parameters (frame size,
sample rate) to be changed at any frame. We handle this, so
you should also take care of it
here, we use a stream based decoder (mpeg1video), so we
feed decoder and see if it could decode a frame

https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decode_video.c