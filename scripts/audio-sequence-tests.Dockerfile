# Offline 32-bit ARM synthesis only; no 3DS services or shared emulator.
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-linux-gnueabihf libc6-dev-armhf-cross qemu-user python3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /work
ENV CC=arm-linux-gnueabihf-gcc \
    ASAN_OPTIONS=detect_leaks=0 \
    UBSAN_OPTIONS=halt_on_error=1
