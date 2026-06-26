# Stage 1: Build Environment
FROM fedora:latest AS builder

# Prevent interactive prompts during installation (dnf handles this natively,
# but it's good practice to keep the environment clean)
ENV DEBIAN_FRONTEND=noninteractive

# Install essential build tools
# Note: 'build-essential' is replaced by 'gcc', 'gcc-c++', 'make'
# Note: 'libsqlite3-dev' -> 'sqlite-devel'
# Note: 'libreadline-dev' -> 'readline-devel'
# Note: 'libserialport-dev' -> 'libserialport-devel'
RUN dnf -y install \
    gcc \
    gcc-c++ \
    make \
    cmake \
    gdb \
    git \
    pkgconfig \
    curl \
    vim \
    less \
    sqlite-devel \
    readline-devel \
    && dnf clean all

# Set up a non-root user
RUN useradd -m -s /bin/bash devuser
USER devuser

# Add alias to .bashrc
RUN echo "alias ll='ls -la --color=auto'" >> /home/devuser/.bashrc

ENV ARTIFACTS_ROOT_DIR=/tmp/sercon-artifacts

WORKDIR /sercon

CMD ["/bin/bash"]
