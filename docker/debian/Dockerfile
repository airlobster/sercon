# Stage 1: Build Environment
FROM debian:bookworm-slim AS builder

# Prevent interactive prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Install essential build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    gdb \
    git \
    pkg-config \
		curl \
		vim \
		less \
		libsqlite3-dev \
    libreadline-dev \
    libserialport-dev

RUN rm -rf /var/lib/apt/lists/*

# Set up a non-root user
RUN useradd -m -s /bin/bash devuser
USER devuser

# Add this near the end of your Dockerfile
RUN echo "alias ll='ls -la --color=auto'" >> /home/devuser/.bashrc

# RUN useradd -m -s /bin/bash appuser
WORKDIR /sercon

CMD ["/bin/bash"]
