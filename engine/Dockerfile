# Linux image for the epoll backend. Not used on the Windows host build.
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake ninja-build ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY engine ./engine
RUN cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build

FROM ubuntu:24.04
COPY --from=build /src/build/engine/helix_engine /usr/local/bin/helix_engine
ENV HELIX_BIND=0.0.0.0 HELIX_PORT=9001
EXPOSE 9001
CMD ["helix_engine"]
