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

$ ./moshthese ~/Movies/bobble_vector.mov ~/Movies/image.mov ~/desktop/moshpyt$(date +%s).mov
```
