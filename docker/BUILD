#!/usr/bin/env bash

function error() {
	echo "Error: $@" >&2
	exit 1
}

linux=$1
[ -n "$linux" ] || error "Missing linux version argument"

cd "$(dirname "$0")"
[ -d "${linux}" ] || error "Linux version directory '${linux}' does not exist"

docker build -t dev-sercon-${linux,,} ${linux}/.

