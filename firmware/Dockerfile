# Builds the firmware ELF with the ARM GCC toolchain, then runs it under
# QEMU in a slim runtime image. The UART bridge listens on 0.0.0.0:5678 so
# other containers on the compose network (the backend) can reach it --
# see progress.md for why this is a TCP socket rather than a real PTY.

FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    cmake \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -G Ninja -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-toolchain.cmake . \
    && cmake --build build-arm --target helm

FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    qemu-system-arm \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build-arm/helm.elf /firmware/helm.elf

EXPOSE 5678

CMD ["qemu-system-arm", "-M", "mps2-an385", "-nographic", \
     "-kernel", "/firmware/helm.elf", \
     "-chardev", "socket,id=serial0,host=0.0.0.0,port=5678,server=on,wait=off", \
     "-serial", "chardev:serial0", \
     "-monitor", "none"]
