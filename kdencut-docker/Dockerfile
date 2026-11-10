# syntax=docker/dockerfile:1
# KdenCut in a browser: builds MLT 7.42 and KdenCut from this repository
# on top of the linuxserver Selkies web desktop (Ubuntu 26.04 "resolute").
FROM ghcr.io/linuxserver/baseimage-selkies:ubunturesolute

ARG MLT_VERSION=v7.42.0
ARG BUILD_JOBS=0

ENV TITLE=KdenCut \
    NO_GAMEPAD=true \
    DEBIAN_FRONTEND=noninteractive

# Build tools and libraries (same set that was used to build and test KdenCut)
RUN \
  echo "**** enable source packages for build-dep ****" && \
  if [ -f /etc/apt/sources.list.d/ubuntu.sources ]; then \
    sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources; \
  else \
    sed -n 's/^deb /deb-src /p' /etc/apt/sources.list > /etc/apt/sources.list.d/src.list; \
  fi && \
  apt-get update && \
  apt-get build-dep -y kdenlive && \
  apt-get install -y --no-install-recommends \
    git ninja-build cmake build-essential pkg-config \
    libkddockwidgets-qt6-dev libopentimelineio-dev libimath-dev \
    libswscale-dev libavfilter-dev libavdevice-dev libavformat-dev libavcodec-dev \
    libxml2-dev libsdl2-dev libsox-dev frei0r-plugins-dev frei0r-plugins libfftw3-dev \
    librubberband-dev libsamplerate0-dev libvorbis-dev ladspa-sdk libebur128-dev \
    libexif-dev libqt6svg6-dev libjack-jackd2-dev liblilv-dev \
    breeze breeze-icon-theme qt6-svg-plugins qt6-image-formats-plugins \
    qml6-module-org-kde-desktop qml6-module-qtquick-shapes qml6-module-qtqml-models \
    qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-window \
    libkf6i18nqml6 kded6 fonts-noto-core ffmpeg mediainfo && \
  rm -rf /var/lib/apt/lists/*

# MLT (Kdenlive master needs >= 7.42, Ubuntu ships older)
RUN \
  J=$([ "$BUILD_JOBS" -gt 0 ] && echo "$BUILD_JOBS" || nproc) && \
  git clone --depth 1 -b "$MLT_VERSION" https://github.com/mltframework/mlt.git /tmp/mlt && \
  cmake -S /tmp/mlt -B /tmp/mlt-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DMOD_QT=OFF -DMOD_QT6=ON -DMOD_GLAXNIMATE=OFF -DMOD_GLAXNIMATE_QT6=OFF \
    -DSWIG_PYTHON=OFF -DMOD_OPENCV=OFF -DMOD_MOVIT=OFF -DBUILD_TESTING=OFF \
    -DMOD_SPATIALAUDIO=OFF -DMOD_GDK=OFF -DMOD_DECKLINK=OFF \
    -DMOD_RNNOISE=OFF -DMOD_RTAUDIO=OFF -DMOD_PLACEBO=OFF -DMOD_OPENFX=OFF \
    -DMOD_VIDSTAB=OFF -DMOD_XINE=OFF && \
  ninja -C /tmp/mlt-build -j "$J" && \
  ninja -C /tmp/mlt-build install && \
  ldconfig && \
  rm -rf /tmp/mlt /tmp/mlt-build

# KdenCut itself (this repository)
COPY . /tmp/kdencut
RUN \
  J=$([ "$BUILD_JOBS" -gt 0 ] && echo "$BUILD_JOBS" || nproc) && \
  cmake -S /tmp/kdencut -B /tmp/kdencut-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DCMAKE_PREFIX_PATH=/usr/local -DCMAKE_INSTALL_PREFIX=/usr \
    -DKDE_INSTALL_USE_QT_SYS_PATHS=ON && \
  ninja -C /tmp/kdencut-build -j "$J" && \
  ninja -C /tmp/kdencut-build install && \
  ldconfig && \
  rm -rf /tmp/kdencut /tmp/kdencut-build /root/.cache

# Autostart KdenCut in the web desktop
COPY kdencut-docker/root /
RUN python3 /usr/local/share/kdencut/install-web-ui.py

EXPOSE 3000 3001
VOLUME /config
