# Security review and release boundary

Verdict: **offline setup/contract harness eligible; live account operations BLOCKED**.
This is an implementation self-review, not an independent reviewer approval. Parent
integration review must use the final file hashes and evidence. No live Apple/iPhone
acceptance has been performed.

## Pinned upstream inspection, before reuse

All reviewed public bytes are indexed in `source.lock.json`. Repository, exact commit,
path and SHA-256 are explicit; source presence/hash equality is not execution approval.
The 84-file bundle includes selected host/auth/cloud/recovery sources, protobuf schema
and generated source, requirements locks, public keychain root certificates, licenses
and patches. It is deliberately NOT described as the full executable import closure.

| Component | Inspected decisive source/contract | Finding / disposition |
|---|---|---|
| candidate host registration | `host/register.py`, `records.py`, `fresh_protection.py`, `zone_signing.py` | Upstream **prepare** is NOT offline: opens keychain, requests PET, lists recovery paths, spends an owner device passcode, reads key shares/views and BeaconStore records. Fresh PCS construction is experimental handwritten protocol code. Not imported or reused as auth/crypto here. Template mode is not offered. |
| candidate cloud writes | `host/cloud.py:12-18,21-31,71-155` | Exactly two new IDs, CREATE, per-write checkpoint/readback exist upstream. However failure can automatically remove records; delete uses `_post_operation` and hand-framed field214, not an installed public delete method. Only local fingerprint structure is adapted, adding full zone/identifier binding. No removal framing or automatic cleanup reused. |
| candidate complete snapshot | `host/snapshot.py` | Starts without token, follows continuation tokens, requires success and COMPLETE, rejects duplicates/nonadvancing pages. Future runtime must preserve that contract. Harness injections are synthetic complete snapshots, not evidence of actual CloudKit completeness. |
| candidate state/limits | `host/common.py`, `service_limits.py`, `recovery_status.py` | State defaults inside checkout; path protections insufficient for this use; service-limits separately use XDG state. Errors permit variable class names. Replaced here with outside-Git/0700 home, O_NOFOLLOW/0600 journal, installation flock, fixed CLI error allowlist; no class-name/raw-error persistence. |
| candidate login | `host/login.py`, `auth_entry.py` | Invokes machine provisioning subprocess, then auth probe with auto-resync/SMS and persistence paths. Hidden-code wrapper recognizes exact prompt strings. No reuse or execution. Future owner interaction needs a bounded separately admitted child and tested FD suppression, not inherited logging behavior. |
| dependency acquisition | `scripts/setup.sh`, `host/install_findmy.py`, `fetch_coreadi.py`, patches/locks | Setup clones/mutates os-findmy-e, creates Python env, installs packages, builds/installs patched FindMy and downloads/extracts Apple runtime data. All-in-one installer additionally reads/writes board flash and builds key-bearing firmware. Neither installer was run; none is called by this tool. |
| FindMy cloud client | `findmy/cloudkit/client.py:480-545,616-682`; `constants.py:53-58` | `record_save` defaults **UPDATE**. Explicit CREATE=2 is mandatory. It serializes merge=true and a gzip/protobuf request then returns server fields; own doc warns echo != write/readability proof. Actual pinned function is run in AST-isolated synthetic transport tests. Whole module/account imports are not executed. |
| FindMy keychain | `findmy/keychain/session.py:297-328,410-493,607-695,774-1002` and recovery/share/escrow sources inspected as text | Open costs PET reauthentication. Recover yields existing device identity in memory, then PCS keys involve wider keychain views. Join/enrol/delete are separate irreversible mutations and MUST remain unreachable. No keychain/auth implementation was reused. Full cryptographic and transitive HTTP audit remains incomplete. |
| os-findmy-e saved auth | `tools/machoadi/login_store.py` | Default keep_password=True; token state plaintext even with password omitted. Temp open can follow symlink, no fsync. Not reused; future runtime requires encrypted envelope and protected atomic persistence. |
| os-findmy-e provider | `tools/machoadi/provider.py:74-87` | Header acquisition invokes subprocess and includes raw child stderr in SystemExit. Not acceptable as public diagnostic boundary; not imported/executed. |
| os-findmy-e native emulator | `tools/machoadi/harness.py:319-328`, stubs/macho/provider sources | Unknown executed import handler can return without implemented semantics. Cannot qualify native auth from apparent success. Exact Apple runtime profile and executed-import/ABI behavior need separate review/tests. No Apple binary downloaded or executed. |
| transitive Anisette | FindMy `reports/anisette.py:18,666-686`; requirements | Imports Anisette at module level; local provider can lazily download native libraries. Package version/hash lock is not full native/source review. Source/artifact closure not approved and not installed. This is an explicit remaining blocker, not a claimed complete audit. |

## Local invariants checked

* No top-level FindMy/Anisette import, subprocess/installer dispatch or credential reader.
* Default commands exercised in a real child with socket/process audit denial; upstream
  imports remain absent. The denial fixture is a test boundary, not an OS sandbox claim.
* Only explicit `fetch-sources --execute --allow-public-source-network` admits fixed
  public GET URLs. Redirects/proxy inheritance disabled; bounded reads and hashes.
* Journal owns exactly two distinct UUIDv4 IDs, ordered required types and stable bound
  digests. Existing pair file is exclusive-create only. Symlink journal reads refused.
* Durable intent (file + directory fsync) precedes each create; apply is never replayed
  after an attempt or uncertainty. No rollback, implicit new identity or deletion.
* Readback compares the entire expected snapshot map; mismatched first record halts
  before the second write. Original baseline changes and ID collisions stop before writes.
* One nonblocking flock covers every async prepare/apply transaction; contention fails.
* UPDATE semantics are rejected and record-save arguments are set explicitly. Tests
  execute the original pinned upstream method against clearly synthetic request objects.
* Exception canaries and unknown CLI arguments never reach stdout/stderr/journal.
  CLI output is fixed allowlisted JSON, not arbitrary upstream exception/class text.
* Files under test fixtures are public MIT source with license, not credentials or SDKs.
  Only selected function AST is executed; this does NOT exercise real protobuf/crypto.

## Limitations not waived by tests

The library adapter is dependency-injected and is not a malicious-code sandbox: a caller
who supplies an arbitrary client can perform arbitrary I/O. Only synthetic clients were
used. The CLI has no path to authenticate or construct a live store. Complete snapshot
quality, immutable candidate-record ownership, HTTP retry behavior, approved installed
cryptographic functions and endpoint admission remain future runtime responsibilities.
No hostile-same-UID filesystem isolation, OS power-loss proof or device acceptance is
claimed. Source downloader has a per-I/O timeout/read cap, not a global wall-clock
network deadline; it handles public source only and never secrets.

`remove` is deliberately rejected, not simulated. The user's owned-record safety
requirements do not authorize a guessed CloudKit delete format or keychain cleanup.
No live apply should be enabled before reviewed conditional removal/reconciliation and
all credential/dependency gates are complete. Preserve uncertain journals for manual
read-only recovery; never infer absent records from partial sync, response echo or a
transport exception.
