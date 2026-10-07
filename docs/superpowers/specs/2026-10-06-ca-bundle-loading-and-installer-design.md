# CA Bundle Loading and Installer Reliability Design

## Context

The bundled firmware-3.73 `libssl.suprx` does not discover the first 67 Sony roots by scanning arbitrary PEM input. Its CA loader seeks to hard-coded byte offsets in `vs0:data/external/cert/CA_LIST.cer`, reads hard-coded lengths, computes SHA-1 over each exact raw PEM slice including its line ending, and compares the result with an embedded 67-entry table. The matching firmware-3.73 CA_LIST is 99,264 bytes, begins its first PEM block at offset 20, and must remain byte-for-byte unchanged at the start of any replacement bundle. Re-serializing its PEM, changing CRLF to LF, removing its labels, or inserting a generated header before it invalidates those lookups.

The browser stack also ingests appended roots through an OpenSSL store path. That path repeatedly calls `PEM_read_bio_X509_AUX` and `X509_STORE_add_cert`, and terminates on any add failure. OpenSSL reports an exact duplicate certificate as an add failure. Therefore the bundle has two independent invariants: preserve the stock 3.73 Sony prefix exactly, and omit duplicate DER certificates from the appended Mozilla section.

Wikipedia requires TLS 1.2 and ECDHE-ECDSA AES-GCM. The bundled 3.73 browser compatibility module supports that protocol and cipher family. The remaining engineering problems are therefore trust-store ingestion, reliable installation, replacement-module activation, and build reproducibility.

## Goals

1. Generate a deterministic bundle with the exact firmware-3.73 CA_LIST image at byte zero and no duplicate DER certificates in the appended section.
2. Test both loader contracts: fixed stock offsets/raw-slice digests and the appended-root OpenSSL API sequence.
3. Make installation, updating, and removal explicit rather than state-dependent toggles.
4. Prevent the installer from reporting success after a failed mount, copy, rename, write, or close.
5. Make module replacement safer and observable, especially for the browser compatibility module.
6. Restore clean builds with the current VitaSDK/GCC toolchain.
7. Add continuous validation so a duplicate bundle or broken VitaSDK build cannot be merged unnoticed.

## Non-goals

- Replacing Sony's browser or OpenSSL implementation.
- Adding TLS 1.3.
- Changing Wikimedia compatibility policy.
- Supporting arbitrary user-provided CA bundles in the Vita UI.
- Reworking the application's graphics layer beyond build-correct declarations.

## Considered approaches

### 1. Replace only `misc/certs.cer`

This is the smallest patch, but it leaves no reproducible generation path, no loader-accurate regression test, the ambiguous toggle installer, unchecked writes, and a brittle kernel path rewrite. A future certificate refresh could silently reintroduce the same failure.

### 2. Generate and validate the bundle, but leave runtime code unchanged

This prevents the duplicate-loader failure and is substantially better than a binary-only update. It still leaves users unable to distinguish install, update, and removal, and it cannot establish that the replacement modules were copied or selected correctly.

### 3. End-to-end reliability update

This design selects the third approach: deterministic generation, exact loader validation, explicit installer actions, transactional file replacement, safer module redirection, current-toolchain fixes, and CI. It is the only approach that addresses every independently observed failure mode without replacing the TLS stack.

## CA source layout and generation

`misc/sony_certs.cer` will contain the preserved Sony/Vita prefix as the authoritative first source. `misc/mozilla_cacert.pem` will contain the pinned Mozilla/curl extract used for reproducible builds. `tools/update_ca_bundle.py` will generate `misc/certs.cer` from those inputs.

The generator will:

1. Parse and validate every PEM certificate from each source.
2. Compute a SHA-256 fingerprint over each DER certificate.
3. Require the fixed Sony source to be internally duplicate-free.
4. Copy the complete Sony source byte-for-byte at offset zero.
5. Keep the first occurrence of every fingerprint and discard later Mozilla duplicates.
6. Append source metadata after the fixed prefix, followed by unique Mozilla certificates in Mozilla order.
7. Emit deterministic LF-terminated PEM only for the appended certificates; the Sony prefix retains its original labels, spaces, and CRLF layout.
8. Report source counts, duplicate counts, and final count.

The update workflow will download `https://curl.se/ca/cacert.pem` with `curl -q`, write the pinned Mozilla input, regenerate the combined file, and run all certificate tests. Network access is required only for an explicit update target, never for normal builds or tests.

## Loader-accurate validation

A small host C test program will open a bundle with OpenSSL and execute the Vita-relevant loop:

1. `PEM_read_bio_X509_AUX` reads one certificate.
2. `X509_STORE_add_cert` adds it to a new store.
3. Any add failure is fatal and prints the certificate index and OpenSSL error.
4. End-of-file is accepted only when no parse error remains.

This catches the exact duplicate behavior missed by ordinary `openssl verify -CAfile` tests. Python unit tests will independently cover parser, ordering, deduplication, deterministic output, malformed PEM rejection, and repository-file consistency.

The real bundle test will assert:

- every input certificate is syntactically valid;
- the generated output contains no duplicate DER fingerprints;
- the output begins with the complete firmware-3.73 Sony CA_LIST byte-for-byte;
- regenerating from the pinned inputs produces a byte-identical `misc/certs.cer`;
- the OpenSSL loader loop adds every emitted certificate;
- ISRG Root X1 and ISRG Root X2 are present in the generated output.

## Installer behavior

The menu will expose explicit actions:

1. Install or update the full package.
2. Install or update the kernel plugin.
3. Install or update the browser compatibility module.
4. Install or update the app HTTP/SSL modules.
5. Install or update the CA bundle.
6. Remove the kernel plugin.
7. Remove the browser compatibility module.
8. Remove the app HTTP/SSL modules.
9. Restore the original CA bundle.
10. Remove the full package.
11. Exit.

No action will change meaning based on the presence of a backup file. Updating the VPK and selecting the CA update action will always attempt to install the VPK's current `certs.cer`.

### Transactional replacement

Every installed file will use the following replacement sequence:

1. Copy the VPK payload to a sibling temporary file with checked partial-read and partial-write loops.
2. Flush and close the temporary file successfully.
3. Rename an existing destination to a rollback file when present.
4. Rename the temporary file into place.
5. Restore the rollback file if activation fails.
6. Remove the rollback file after success.

The original Sony CA bundle remains in `CA_LIST.cer_old` until the user explicitly restores it. CA updates replace only the active iTLS bundle and never overwrite that original backup.

The installer will validate the final destination size against the source size after replacement. A failed operation will show the stage and hexadecimal Vita error code, will not print `Done`, and will not reboot automatically.

### Configuration update

The taiHEN configuration edit will use a NUL-terminated read buffer, write a complete temporary configuration, close it successfully, then rotate it into place. It will not rename the live configuration before a valid replacement exists. Existing iTLS entries will not be duplicated.

## Kernel module redirection

Path selection will be extracted into a small standard-C helper that is host-testable. The helper will:

- reject null and empty paths;
- compare exact paths for `libhttp`, `libssl`, and `np_commerce2`;
- recognize the exact trailing relative path `/webcore/ScePsp2Compat.suprx` independently of AppMgr's opaque mount alias;
- preserve the complete colon-delimited prefix and replace only that trailing path with `/itls/ScePsp2Compat.suprx` through a bounded caller-provided buffer;
- never mutate the module manager's `const char *` input buffer;
- leave paths without a non-empty mount delimiter, unrelated paths, extra-suffix collisions, and undersized-buffer cases unchanged.

Firmware-3.60 `SceAppMgr` generates these mount names from an `sd` prefix and pseudorandom bytes. Their visible characters change across Browser launches, so the hook treats everything before the exact relative module path as opaque and preserves it byte-for-byte.

The kernel plugin will initialize hook IDs to invalid values, release only installed hooks, and persist enabled replacements, hook results, requested paths, selected paths, and load results in `ur0:tai/itls.log` while retaining `ksceDebugPrintf` output.

## Build system

The top-level build will compile the app, kernel plugin, and DNS compatibility plugin before packaging the VPK. C sources will use GNU C99 explicitly so current GCC does not reinterpret empty parameter lists under newer C defaults. The app will use declared current VitaSDK APIs, including `scePowerRequestColdReset`, and correct function prototypes.

Generated build directories and products will be cleaned by the top-level `clean` target and ignored by Git.

## Continuous integration

GitHub Actions will run two jobs:

- Host validation on Ubuntu: Python unit tests, bundle regeneration check, host C tests linked with OpenSSL, and shell/static checks.
- VitaSDK build in the current `vitasdk/vitasdk:latest` container: clean build of every source-built component and VPK packaging.

The workflow will not download certificate data. It validates the pinned source and generated output only.

## Error handling and compatibility

- All filesystem-return values are treated as signed Vita error codes.
- Cleanup failures after a successful activation are warnings; failures that leave the requested destination uninstalled are fatal.
- Existing installations with `CA_LIST.cer_old` are updated in place without replacing the original backup.
- A missing original CA file during first install is fatal because the installer cannot preserve recovery semantics.
- Removal actions are idempotent when their target is already absent.
- The 3.60-only DNS module behavior remains conditional on firmware version.

## Acceptance criteria

1. The generated CA bundle is byte-reproducible and contains no duplicate DER certificates.
2. The loader-accurate test adds every certificate without error.
3. ISRG Root X1 and X2 are reachable in the generated file.
4. A second CA install updates the active bundle rather than restoring/removing it.
5. Any injected copy or rename failure produces an error and suppresses the success/reboot path.
6. Browser redirection preserves any opaque colon-delimited mount prefix while replacing only the exact `/webcore/ScePsp2Compat.suprx` tail, without out-of-bounds access or input mutation.
7. The complete VPK builds with the current VitaSDK container.
8. CI covers both host validation and VitaSDK packaging.
