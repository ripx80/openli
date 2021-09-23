ARG VERSION=1
FROM debian:latest
# https://docs.docker.com/develop/develop-images/dockerfile_best-practices/

ARG VERSION
LABEL org.opencontainers.image.authors="ripx80@nomail"
LABEL tag="openlibase"

RUN apt-get update && apt-get install -y \
    curl \
    build-essential \
    libssl-dev \
    libsqlcipher-dev \
    librabbitmq-dev \
    automake \
    libtool \
    gcc \
    librte-eal21 \
    librte-ethdev21 \
    librte-mbuf21 \
    librte-mempool21 \
    libbpf0 \
    libnuma1 \
    libpcap0.8 \
    libyaml-dev \
    libosip2-dev \
    libzmq3-dev \
    libjudy-dev \
    uthash-dev \
    libmicrohttpd-dev \
    libjson-c-dev \
    libgoogle-perftools-dev

RUN ["/bin/bash", "-c", "set -o pipefail && curl -1sLf 'https://dl.cloudsmith.io/public/wand/libwandder/cfg/setup/bash.deb.sh' | bash"]
RUN ["/bin/bash", "-c", "set -o pipefail && curl -1sLf 'https://dl.cloudsmith.io/public/wand/libtrace/cfg/setup/bash.deb.sh' | bash"]
RUN ["/bin/bash", "-c", "set -o pipefail && curl -1sLf 'https://dl.cloudsmith.io/public/wand/libwandio/cfg/setup/bash.deb.sh' | bash"]
RUN apt-get update && apt-get install -y libtrace4 libwandder2-dev libtrace4-dev && mkdir -p /etc/openli/

COPY . /app
RUN (cd /app && ./bootstrap.sh; ./configure && make && make install)
