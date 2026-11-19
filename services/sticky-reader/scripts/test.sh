#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${SRP_SCRATCH:=/root/.hermes/cache/scratch/sticky-reader-service}"
export GOMAXPROCS=1 GOPROXY=https://proxy.golang.org GOMODCACHE="$SRP_SCRATCH/go-mod-cache" GOCACHE="$SRP_SCRATCH/go-cache" GOTMPDIR="$SRP_SCRATCH" TMPDIR="$SRP_SCRATCH" PYTHONDONTWRITEBYTECODE=1
mkdir -p "$GOCACHE" "$SRP_SCRATCH/artifacts"
go test -mod=readonly -p=1 -count=1 ./...
go test -mod=readonly -race -p=1 -count=1 ./...
go vet -mod=readonly -p=1 ./...
python3 scripts/test_oci.py
python3 scripts/test_deploy.py
python3 scripts/test_live_flow.py
CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -mod=readonly -p=1 -trimpath -o "$SRP_SCRATCH/artifacts/sticky-reader-amd64" ./cmd/sticky-reader
CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -mod=readonly -p=1 -trimpath -o "$SRP_SCRATCH/artifacts/qualify-amd64" ./cmd/qualify
CGO_ENABLED=0 GOOS=linux GOARCH=arm64 go build -mod=readonly -p=1 -trimpath -o "$SRP_SCRATCH/artifacts/sticky-reader-arm64" ./cmd/sticky-reader
python3 scripts/package_oci.py --binary "$SRP_SCRATCH/artifacts/sticky-reader-arm64" --ca /etc/ssl/certs/ca-certificates.crt --output "$SRP_SCRATCH/artifacts/sticky-reader-arm64.oci.tar"

python3 scripts/measure.py --binary "$SRP_SCRATCH/artifacts/qualify-amd64" --output "$SRP_SCRATCH/artifacts/resource.json"

python3 scripts/verify_runtime_deps.py --binary "$SRP_SCRATCH/artifacts/sticky-reader-amd64" --cache "$GOMODCACHE" --sum go.sum --output "$SRP_SCRATCH/artifacts/runtime-dependencies.json"
