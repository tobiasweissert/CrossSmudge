#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${SRP_SCRATCH:=/root/.hermes/cache/scratch/sticky-reader-service}"
: "${1:?supply an absolute new output directory; existing artifacts are immutable}"
case "$1" in /*) ;; *) exit 2;; esac
mkdir "$1"
SRP_REPAIR_OUT=$1
export GOMAXPROCS=1 GOPROXY=off GOSUMDB=sum.golang.org GOMODCACHE="$SRP_SCRATCH/go-mod-cache" GOCACHE="$SRP_SCRATCH/repair-round1/go-cache" GOTMPDIR="$SRP_SCRATCH/repair-round1" TMPDIR="$SRP_SCRATCH/repair-round1" PYTHONDONTWRITEBYTECODE=1
mkdir -p "$GOCACHE"
go test -mod=readonly -p=1 -count=1 -timeout=120s -v ./... > "$SRP_REPAIR_OUT/normal.log" 2>&1
go test -mod=readonly -race -p=1 -count=1 -timeout=120s -v ./... > "$SRP_REPAIR_OUT/race.log" 2>&1
go vet -mod=readonly -p=1 ./... > "$SRP_REPAIR_OUT/vet.log" 2>&1
python3 scripts/test_oci.py > "$SRP_REPAIR_OUT/oci-tests.log" 2>&1
python3 scripts/test_deploy.py > "$SRP_REPAIR_OUT/deploy-tests.log" 2>&1
CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -mod=readonly -p=1 -trimpath -o "$SRP_REPAIR_OUT/sticky-reader-amd64" ./cmd/sticky-reader
CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -mod=readonly -p=1 -trimpath -o "$SRP_REPAIR_OUT/qualify-amd64" ./cmd/qualify
CGO_ENABLED=0 GOOS=linux GOARCH=arm64 go build -mod=readonly -p=1 -trimpath -o "$SRP_REPAIR_OUT/sticky-reader-arm64" ./cmd/sticky-reader
file "$SRP_REPAIR_OUT/sticky-reader-amd64" "$SRP_REPAIR_OUT/sticky-reader-arm64" "$SRP_REPAIR_OUT/qualify-amd64" > "$SRP_REPAIR_OUT/elf.log"
python3 scripts/package_oci.py --binary "$SRP_REPAIR_OUT/sticky-reader-arm64" --ca /etc/ssl/certs/ca-certificates.crt --output "$SRP_REPAIR_OUT/sticky-reader-arm64.oci.tar" > "$SRP_REPAIR_OUT/oci.json"
python3 scripts/measure.py --binary "$SRP_REPAIR_OUT/qualify-amd64" --output "$SRP_REPAIR_OUT/resource.json" > "$SRP_REPAIR_OUT/resource.log" 2>&1
python3 scripts/verify_runtime_deps.py --binary "$SRP_REPAIR_OUT/sticky-reader-amd64" --cache "$GOMODCACHE" --sum go.sum --output "$SRP_REPAIR_OUT/runtime-dependencies.json" > "$SRP_REPAIR_OUT/runtime-dependencies.log" 2>&1
go version -m "$SRP_REPAIR_OUT/sticky-reader-amd64" > "$SRP_REPAIR_OUT/build-info.txt"
