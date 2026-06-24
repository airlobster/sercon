FROM ubuntu:24.04

RUN apt-get update -y

RUN apt-get install -y \
		vim \
		less \
		curl \
    software-properties-common \
    unminimize \
    man-db \
    manpages \
    manpages-dev \
    manpages-posix-dev \
    curl \
    vim-tiny \
    net-tools \
    build-essential \
    libreadline-dev \
    libserialport-dev \
    sqlite3 \
    libsqlite3-dev \
    git \
    doxygen \
    graphviz \
    xdg-utils
		
RUN yes | unminimize
RUN apt-get upgrade -y
RUN apt-get clean && rm -rf /var/lib/apt/lists/*

RUN echo "alias ll='ls -la --color=auto'" >> /etc/bash.bashrc

RUN groupadd -r appusers && useradd -r -g appusers -u 1001 appuser
USER appuser

WORKDIR /sercon
ENV ARTIFACTS_ROOT_DIR=/tmp/sercon-artifacts

# to build:
# > docker build -t my-ubuntu -f Dockerfile.ubuntu .
# to run:
# > docker run -it -v ~/sercon:/sercon my-ubuntu bash
