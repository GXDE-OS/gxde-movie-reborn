# GXDE movie

GXDE movie is GXDE Desktop Environment Movie Player.

It also provide libgxmr to provide video play service for dde-file-manager/dde-desktop

## Dependencies

### Build dependencies

* cmake
* qt5
* ffmpeg
* mpv

### Runtime dependencies

* Qt5 (>= 5.3)
* Qt5-X11extras
* mpv

## Installation

### Build from source code

1. Make sure you have installed all dependencies.

2. Build:
```
$ cd gxde-movie-reborn
$ mkdir Build
$ cd Build
$ cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr ..
$ make
```

3. Install:
```
$ sudo make install
```

When install complete, the executable binary file is placed into `/usr/bin/deepin-movie`.

