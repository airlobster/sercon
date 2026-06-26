# Use AlmaLinux 9 as the base (binary compatible with RHEL/CentOS)
FROM almalinux:9

# Update and install required development libraries
# 'dnf' is the modern package manager for RHEL/CentOS systems
# Update and install required development libraries
RUN dnf install -y epel-release && \
    dnf install -y --setopt=install_weak_deps=False --allowerasing \
    gcc-c++ \
    make \
    cmake \
    gdb \
    git \
    pkgconfig \
    sqlite-devel \
    readline-devel \
    vim \
    curl \
    less \
    && dnf clean all

# Set up user and alias
RUN useradd -m -s /bin/bash devuser && \
    echo "alias ll='ls -la --color=auto'" >> /home/devuser/.bashrc

USER devuser
WORKDIR /sercon

ENV ARTIFACTS_ROOT_DIR=/tmp/sercon-artifacts

# Default to bash session
CMD ["/bin/bash"]
