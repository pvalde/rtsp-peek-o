# rtsp-peek

## Overview
A CLI-driven RTSP stream viewer written in C. Leverages FFmpeg for decoding and SDL2 for low-latency rendering. View multiple streams in a single vertical stack.

**Features**:
- Minimal dependencies (only FFmpeg and SDL2)
- Tested on Linux systems (Fedora)

## Dependencies & Build

### 1. Install Dependencies

This project has been built and tested with the following system libraries:
- SDL2 >= 2.32.64
- FFmpeg:
  - libavcodec >= 61.19.101
  - libavformat >= 61.7.100
  - libavutil >= 59.39.100
  - libswscale >= 8.3.100

Other versions may work, but these are the versions that have been verified.

Note: Only development packages include the headers and pkg-config files required to build this project. Runtime packages alone are not sufficient.

**Debian / Ubuntu**:
```
sudo apt update
sudo apt install -y build-essential pkg-config git \
    libsdl2-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
```
**Fedora**:
```
sudo dnf install -y gcc make pkgconf-pkg-config git \
    SDL2-devel ffmpeg-devel
```

Check installed versions:
```
pkg-config --modversion sdl2
pkg-config --modversion libavcodec
pkg-config --modversion libavformat
pkg-config --modversion libavutil
pkg-config --modversion libswscale
```


### 2. Clone the Project
```
git clone https://github.com/pvalde/rtsp-peek
cd rtsp-peek
```

### 3. Build
```
# release
make BUILD_TYPE=release

# debug
make BUILD_TYPE=debug
```

### 4. Install (optional)
```
# Install release binary to ~/.local/bin (default)
make install

# Specify a custom prefix
make install PREFIX=/usr/local
```

### 5. Run
```
# if installed
rtsp-peek 'rtsp://example.com/stream'

# from cloned dir
./bin/release/rtsp-peek 'rtsp://example.com/stream'
```

## Usage


## License


This program dynamically links against the following third-party libraries,
provided by the user’s system:

- SDL2 - zlib license.
- FFmpeg (libavformat, libavcodec, libavutil, libswscale) - LGPL or GPL license.

No external library code is included in this repository.
