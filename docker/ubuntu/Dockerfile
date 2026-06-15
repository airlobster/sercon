FROM ubuntu

RUN apt update -y

RUN apt install -y \
	software-properties-common \
	unminimize \
	man-db \
	manpages \
	manpages-dev \
	manpages-posix-dev \
	curl \
	vim \
	net-tools \
	build-essential \
	libreadline-dev \
	libserialport-dev \
	git

RUN yes | unminimize

RUN apt upgrade -y

RUN echo "alias ll='ls -la --color=auto'" >> /etc/bash.bashrc

RUN groupadd -r appusers && useradd -r -g appusers -u 1001 appuser
USER appuser

WORKDIR /sercon
ENV ARTIFACTS_ROOT_DIR=/tmp/sercon-artifacts

# to build:
# > docker build -t my-ubuntu -f Dockerfile.ubuntu .
# to run:
# > docker run -it -v ~/sercon:/sercon my-ubuntu bash
