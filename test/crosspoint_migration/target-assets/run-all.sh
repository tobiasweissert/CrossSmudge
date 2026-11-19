#!/bin/sh
# Routine bounded regression entrypoint; callers supply durable evidence path.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${1:?usage: sh run-all.sh EVIDENCE_DIRECTORY}"
for case in assets png png-preserve; do
  flock /root/.hermes/cache/scratch/crosspoint-qualification.lock \
    python3 "$here/run.py" --case "$case" --phase final --evidence "$1"
done
