#! /bin/bash
set -e

export PYLINTHOME="${PYLINTHOME:-/tmp/obs-avs-pylint}"

if command -v pylint >/dev/null 2>&1; then
	pylint -d C0103,C0209 ./*.py
else
	python3 -m pylint -d C0103,C0209 ./*.py
fi
