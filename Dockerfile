ARG ARCH=${ARCH:-x86}

FROM --platform=linux/amd64 ubuntu:20.04 AS x86
FROM --platform=linux/arm64 nvcr.io/nvidia/l4t-base:r32.6.1 AS jetson

FROM ${ARCH} AS install-dependency
ARG ARCH
ENV ENV=${ARCH}
RUN apt-get update && apt-get install -y \
	cmake \
	g++ \
	libssl-dev \
	&& rm -rf /var/lib/apt/lists/*

FROM install-dependency AS build-stage
ARG ARCH
ENV ENV=${ARCH}
COPY . /app
WORKDIR /app
RUN mkdir build
WORKDIR /app/build
RUN /usr/bin/cmake -DBUILD_FROM_SDK_SRC=ON -DARCH=${ARCH} ..
RUN /usr/bin/cmake --build . --target package


FROM scratch AS output-stage
ARG ARCH
ENV ENV=${ARCH}
COPY --from=build-stage /app/*.tar.gz / 