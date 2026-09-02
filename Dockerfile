# Control Station G29 reader
# Base: Ubuntu 20.04 LTS — matches ROS 1 Noetic hosts (user-selected).
# Compiler: g++-10 because focal's default g++-9 does not provide usable C++20.
# This image builds and mock-runs on Mac Docker. It cannot see a USB G29 on
# Docker Desktop for Mac (no HID passthrough). Bind /dev/input on Ubuntu.
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=UTC

# g++-10: C++20 on Ubuntu 20.04 without a toolchain PPA.
# libevdev: typed wrapper over /dev/input/event* (same layer SDL/joy use).
# cmake/pkg-config: build. evtest: optional host-side calibration (also here
# so `docker compose run g29-reader evtest` works on Ubuntu).
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates \
      cmake \
      make \
      g++-10 \
      pkg-config \
      libevdev-dev \
      evtest \
      udev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt /src/CMakeLists.txt
COPY include /src/include
COPY src /src/src
COPY config /src/config

RUN cmake -S /src -B /src/build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++-10 \
    && cmake --build /src/build --parallel \
    && cmake --install /src/build \
    && mkdir -p /etc/cs_g29_reader \
    && cp /src/config/g29_mapping.conf /etc/cs_g29_reader/g29_mapping.conf

WORKDIR /work
# Default: mock device so `docker compose up` works on a Mac with no G29.
# On Ubuntu with the wheel attached, override the command (see README).
ENTRYPOINT ["g29_reader"]
CMD ["--mock", "--config", "/etc/cs_g29_reader/g29_mapping.conf"]
