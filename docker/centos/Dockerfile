FROM rockylinux/rockylinux:9

# Enable additional repositories for latest dev tools
RUN dnf install -y dnf-plugins-core && dnf config-manager --set-enabled powertools || true

# Clean and update the cache
RUN dnf clean all && dnf makecache

RUN dnf groupinstall -y "Development Tools"
RUN dnf install -y --allowerasing \
		vim \
		curl \
		man-pages \
		man-db \
		net-tools \
		readline-devel \
		graphviz \
		cmake \
		sqlite \
		sqlite-devel \
		glibc-langpack-en \
		glibc-devel \
		glibc-headers \
		glibc-doc

# Build libserialport from source (not in Rocky 9 repos)
RUN dnf install -y autoconf automake libtool pkgconfig && \
	cd /tmp && \
	curl -L https://sigrok.org/download/source/libserialport/libserialport-0.1.1.tar.gz -o libserialport.tar.gz && \
	tar xzf libserialport.tar.gz && \
	cd libserialport-0.1.1 && \
	./configure && \
	make && \
	make install && \
	ldconfig && \
	cd /tmp && rm -rf libserialport* && \
	dnf remove -y autoconf automake libtool && \
	dnf clean all

RUN dnf clean all && rm -rf /var/cache/dnf && \
	localedef -i en_US -f UTF-8 en_US.UTF-8 || true

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH

RUN groupadd -r appusers && useradd -r -m -g appusers -u 1001 appuser
USER appuser

RUN touch /home/appuser/.bashrc
RUN echo "alias ll='ls -la --color=auto'" >> /home/appuser/.bashrc

WORKDIR /sercon
ENV ARTIFACTS_ROOT_DIR=/tmp/sercon-artifacts

CMD ["/bin/bash"]
