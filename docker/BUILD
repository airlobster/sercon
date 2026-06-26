#!/usr/bin/env bash

function error() {
	echo "Error: $@" >&2
	exit 1
}

linux=$1
[ -n "$linux" ] || error "Usage: $0 <linux-distro>"

cd "$(dirname "$0")"
[ -f "Dockerfile.${linux}" ] || error "No Dockerfile found for '${linux}'"

docker build -t dev-sercon-${linux,,} -f "Dockerfile.${linux}" .

