# Changelog

## Unreleased

### Added

- Public keyless SRP1 conversion service at sticky.tobiasweissert.ch with public-DNS-pinned fetching,
  semantic reader chunks, fragments/search, safe GET forms, on-demand monochrome PNGs and bounded
  shared cache/quotas. Legacy Authorization cannot mint identities or budgets. No verifier setup.
- Owned child cancellation, serial admission, app-scoped deployment templates and validated
  daemonless ARM64 OCI packaging with synthetic protocol and native qualification fixtures.

### Fixed

- Reject HTTP/1 requests before draining unauthenticated, rate-limited, oversized or unsupported-route bodies; enforce the 2048-byte JSON limit without connection reuse after rejection.
- Reject noncanonical nested find cursors and repeated or stacked upstream content encodings.
- Validate every chunk against decoded and escaped JSON budgets, retain private cache ownership through deletion/publication failures, and stop admission until cleanup succeeds.
- Find text across paragraph continuation chunks and preserve source whitespace across styled text, links and fragment positions.
