# CA Bundle Loading and Installer Reliability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make CA updates effective in the Vita browser, make installation/update/removal reliable and explicit, and restore reproducible current-VitaSDK builds.

**Architecture:** Preserve the exact firmware-3.73 CA_LIST image as a fixed byte prefix, append a pinned Mozilla source through a deterministic DER-fingerprint deduplicator, validate the prefix against `libssl`'s fixed offset/length/raw-SHA-1 contract, and validate the appended roots with the OpenSSL read/add loop used by `ScePsp2Compat`. Refactor runtime code around explicit actions and transactional file replacement, while extracting kernel path selection into a standard-C unit that can be host-tested.

**Tech Stack:** C99, VitaSDK, Python 3 standard library, OpenSSL host API, GNU Make, CMake, GitHub Actions, Docker.

---

## File map

- `misc/sony_certs.cer`: exact 99,264-byte firmware-3.73 CA_LIST image.
- `misc/mozilla_cacert.pem`: pinned curl/Mozilla CA source.
- `misc/certs.cer`: generated fixed-prefix and deduplicated-appended combined bundle.
- `tools/update_ca_bundle.py`: deterministic fixed-prefix copier, parser, deduplicator, and generator.
- `tools/update_ca_bundle.sh`: explicit network refresh wrapper using `curl -q`.
- `tests/test_update_ca_bundle.py`: generator, fixed-layout, and repository-consistency tests.
- `tests/ca_loader_test.c`: appended-root OpenSSL loader-loop regression test.
- `tests/test_kernel_path_rewrite.c`: host test for module path selection.
- `kernel/path_rewrite.c`, `kernel/path_rewrite.h`: pure path-selection unit.
- `kernel/main.c`: kernel hook integration and diagnostics.
- `main.c`: explicit installer actions and checked transactional operations.
- `graphics.h`: corrected declarations.
- `old_dns_fix/CMakeLists.txt`, `old_dns_fix/main.c`: current-C compatibility and safe hook lifecycle.
- `Makefile`: complete build/test/update orchestration.
- `.gitignore`: generated build products.
- `.github/workflows/ci.yml`: host and VitaSDK validation.
- `README.md`: updated behavior and maintenance instructions.

### Task 1: Establish failing baselines

**Files:**
- Test: `tests/test_update_ca_bundle.py`
- Test: `tests/ca_loader_test.c`
- Test: `tests/test_kernel_path_rewrite.c`

- [ ] **Step 1: Capture the current VitaSDK build failure**

Run:

```sh
docker run --rm -v "$PWD:/src" -w /src vitasdk/vitasdk:latest sh -lc 'make clean && make'
```

Expected: non-zero exit from current source/toolchain incompatibilities or missing prerequisite module products.

- [ ] **Step 2: Write generator API tests before the generator exists**

Create tests that import `tools.update_ca_bundle` and exercise a wished-for API:

```python
from tools.update_ca_bundle import BundleError, build_bundle, parse_certificates

result = build_bundle((sony_bytes, mozilla_bytes))
assert [entry.der_sha256 for entry in result.certificates] == expected_fingerprints
assert result.duplicate_count == 1
```

Cover first-occurrence ordering, duplicate removal across sources, deterministic bytes, malformed input rejection, and exact repository regeneration.

- [ ] **Step 3: Run the Python test and verify RED**

Run:

```sh
python3 -m unittest -v tests.test_update_ca_bundle
```

Expected: import failure because `tools/update_ca_bundle.py` does not exist.

- [ ] **Step 4: Write the loader regression harness**

The harness must use:

```c
while ((cert = PEM_read_bio_X509_AUX(bio, NULL, NULL, NULL)) != NULL) {
    parsed++;
    if (X509_STORE_add_cert(store, cert) != 1) {
        ERR_print_errors_fp(stderr);
        return EXIT_FAILURE;
    }
    added++;
    X509_free(cert);
}
```

It must clear and inspect the final PEM EOF error, print `parsed=<n> added=<n>`, and fail on any add or parse error.

- [ ] **Step 5: Compile and run the harness against the current bundle to verify RED**

Run:

```sh
cc tests/ca_loader_test.c $(pkg-config --cflags --libs openssl) -o tests/ca_loader_test
tests/ca_loader_test misc/certs.cer
```

Expected: non-zero exit at certificate 68 with `cert already in hash table`.

- [ ] **Step 6: Write kernel path-selection tests against the desired API**

Use `itls_select_replacement_path(const ItlsReplacementState *, const char *)` and assert exact replacements, short-path safety, suffix behavior, prefix-collision rejection, null handling, and unchanged unrelated paths.

- [ ] **Step 7: Compile the path test to verify RED**

Run:

```sh
cc -std=c99 -Wall -Wextra -Werror tests/test_kernel_path_rewrite.c kernel/path_rewrite.c -o tests/test_kernel_path_rewrite
```

Expected: failure because the path-rewrite unit does not exist.

### Task 2: Implement deterministic CA generation

**Files:**
- Create: `tools/__init__.py`
- Create: `tools/update_ca_bundle.py`
- Create: `misc/sony_certs.cer`
- Create: `misc/mozilla_cacert.pem`
- Modify: `misc/certs.cer`
- Test: `tests/test_update_ca_bundle.py`

- [ ] **Step 1: Extract the authoritative Sony prefix**

Extract the complete 99,264-byte firmware-3.73 CA_LIST image from the v3.0.1 repository state into `misc/sony_certs.cer`. Do not derive it from current `origin/master`: that file replaced the expired DST Root CA X3 in place and shifted the fixed offsets expected by the bundled firmware-3.73 `libssl.suprx`.

- [ ] **Step 2: Pin the current Mozilla source**

Download the current curl bundle once with:

```sh
curl -q -fsSL https://curl.se/ca/cacert.pem -o misc/mozilla_cacert.pem
```

Record its extract date and SHA-256 in the generated output header.

- [ ] **Step 3: Implement strict parsing**

`parse_certificates(source_name, data)` must:

```python
pem_blocks = PEM_RE.findall(normalized_text)
der = ssl.PEM_cert_to_DER_cert(pem_block.decode("ascii"))
fingerprint = hashlib.sha256(der).hexdigest()
```

Reject unmatched begin/end markers, zero-certificate sources, non-ASCII PEM, and invalid base64/DER.

- [ ] **Step 4: Implement first-occurrence deduplication**

`build_bundle(sources)` must maintain `seen: set[str]`, append a certificate only when its DER SHA-256 is new, and accumulate per-source and total duplicate counts.

- [ ] **Step 5: Implement deterministic rendering**

Render the fixed Sony source first without changing a byte. Append the ASCII metadata header and normalized unique Mozilla PEM blocks after that prefix, with a final newline. Include source names, source certificate counts, skipped duplicate counts, and output count.

- [ ] **Step 6: Add a command-line interface**

Support:

```sh
python3 tools/update_ca_bundle.py \
  --sony misc/sony_certs.cer \
  --mozilla misc/mozilla_cacert.pem \
  --output misc/certs.cer \
  --check
```

`--check` compares generated bytes to the existing output and exits non-zero without modifying it; without `--check`, write through a temporary file and `os.replace`.

- [ ] **Step 7: Run unit tests to verify GREEN**

Run:

```sh
python3 -m unittest -v tests.test_update_ca_bundle
```

Expected: all tests pass.

- [ ] **Step 8: Regenerate and validate the real bundle**

Run:

```sh
python3 tools/update_ca_bundle.py --sony misc/sony_certs.cer --mozilla misc/mozilla_cacert.pem --output misc/certs.cer
python3 tools/update_ca_bundle.py --sony misc/sony_certs.cer --mozilla misc/mozilla_cacert.pem --output misc/certs.cer --check
tests/ca_loader_test misc/certs.cer
```

Expected: byte-identical check and `parsed=<final> added=<final>`.

- [ ] **Step 9: Commit the generator and bundle**

```sh
git add tools tests/test_update_ca_bundle.py tests/ca_loader_test.c misc/sony_certs.cer misc/mozilla_cacert.pem misc/certs.cer
git commit -m "fix: generate a duplicate-free CA bundle"
```

### Task 3: Implement and test safe kernel path selection

**Files:**
- Create: `kernel/path_rewrite.h`
- Create: `kernel/path_rewrite.c`
- Modify: `kernel/CMakeLists.txt`
- Modify: `kernel/main.c`
- Test: `tests/test_kernel_path_rewrite.c`

- [ ] **Step 1: Define the replacement state and API**

```c
typedef struct ItlsReplacementState {
    int browser;
    int app_tls;
    int commerce;
} ItlsReplacementState;

const char *itls_select_replacement_path(
    const ItlsReplacementState *state,
    const char *path,
    char *rewrite_buffer,
    size_t rewrite_buffer_size
);
```

- [ ] **Step 2: Implement exact and suffix-safe matching**

Use exact `strcmp` for system module paths. For Browser paths, require a non-empty colon-delimited prefix and the exact trailing `/webcore/ScePsp2Compat.suprx` path, preserve the opaque prefix, and append `/itls/ScePsp2Compat.suprx` in the bounded caller-provided buffer. Never modify `path`.

- [ ] **Step 3: Run the path test to verify GREEN**

Run:

```sh
cc -std=c99 -Wall -Wextra -Werror tests/test_kernel_path_rewrite.c kernel/path_rewrite.c -o tests/test_kernel_path_rewrite
tests/test_kernel_path_rewrite
```

Expected: zero exit.

- [ ] **Step 4: Integrate with the kernel hook**

Replace inline string rewriting with the bounded helper and a stack-local rewrite buffer. Initialize hook IDs to `-1`, release only IDs `>= 0`, and log discovered replacement state, requested and selected paths, and module-load results.

- [ ] **Step 5: Add `path_rewrite.c` to the kernel target and commit**

```sh
git add kernel tests/test_kernel_path_rewrite.c
git commit -m "fix: make module redirection safe and testable"
```

### Task 4: Refactor installer actions and file operations

**Files:**
- Modify: `main.c`
- Modify: `graphics.h`

- [ ] **Step 1: Replace state-dependent menu strings with explicit actions**

Define an enum with install/update, remove/restore, full-package, and exit actions. Render the same label regardless of current installation state.

- [ ] **Step 2: Implement checked low-level helpers**

Add helpers with negative Vita error returns:

```c
static int copy_file_checked(const char *source, const char *destination);
static int replace_file_transactional(const char *source, const char *destination);
static int remove_file_if_present(const char *path);
static int install_ca_bundle(void);
static int restore_ca_bundle(void);
```

`copy_file_checked` must loop over reads and partial writes, detect allocation/open/read/write/close errors, and remove incomplete output.

- [ ] **Step 3: Implement transactional replacement**

Write `destination.new`, rotate an existing destination to `destination.rollback`, activate the new file, restore rollback on failure, and remove rollback on success. Verify source and destination sizes.

- [ ] **Step 4: Preserve the original CA backup**

On first CA install, rename the current `CA_LIST.cer` to `CA_LIST.cer_old` only after the new temporary bundle is complete. On subsequent updates, leave `CA_LIST.cer_old` untouched and replace only the active bundle. Restore is explicit and idempotent.

- [ ] **Step 5: Make taiHEN config editing transactional**

Read `ur0:tai/config.txt` into a size-plus-one buffer, NUL-terminate it, write a complete `.new` file, close it, rotate the live file, and roll back on any failure. Detect either the marker or plugin line without reading beyond the buffer.

- [ ] **Step 6: Centralize action execution and result reporting**

Return the first negative error. Print the failing stage and error code. Reboot only when every requested operation succeeds. Removal of absent files is success.

- [ ] **Step 7: Correct current API and prototype usage**

Use `int work(void)`, call `work()`, include `<psp2/power.h>`, call `scePowerRequestColdReset`, and declare `psvDebugScreenSetXY(int x, int y)`.

- [ ] **Step 8: Compile the app with strict warnings**

Run the app build with `-std=gnu99 -Wall -Wextra`. Fix all warnings attributable to modified code.

- [ ] **Step 9: Commit installer changes**

```sh
git add main.c graphics.h
git commit -m "fix: make iTLS installation explicit and transactional"
```

### Task 5: Repair and orchestrate complete VitaSDK builds

**Files:**
- Modify: `Makefile`
- Modify: `kernel/CMakeLists.txt`
- Modify: `old_dns_fix/CMakeLists.txt`
- Modify: `old_dns_fix/main.c`
- Create/modify: `.gitignore`

- [ ] **Step 1: Set GNU C99 explicitly in every source-built target**

Use `-std=gnu99` for top-level, kernel, and DNS plugin C compilation.

- [ ] **Step 2: Make DNS hook lifecycle explicit**

Initialize the hook ID to `-1`, null-check `serverName`, and release only a successfully installed hook.

- [ ] **Step 3: Add component build targets**

The top-level Makefile must build `kernel/itls.skprx` and `old_dns_fix/old_dns_fix.suprx` through out-of-source CMake directories before packaging.

- [ ] **Step 4: Make packaging dependencies real**

Declare every VPK payload as a prerequisite so `make -j` cannot package stale or absent products.

- [ ] **Step 5: Add host test and certificate targets**

Provide:

```make
host-tests:
check-certs:
update-certs:
```

`update-certs` invokes `tools/update_ca_bundle.sh`; ordinary `all` and `test` remain network-free.

- [ ] **Step 6: Clean every generated product**

Remove app products, host test binaries, and component build directories. Add them to `.gitignore`.

- [ ] **Step 7: Run the complete VitaSDK build to verify GREEN**

Run:

```sh
docker run --rm -v "$PWD:/src" -w /src vitasdk/vitasdk:latest sh -lc 'make clean && make -j2'
```

Expected: zero exit and an `iTLS-Enso.vpk` containing every declared payload.

- [ ] **Step 8: Inspect VPK contents**

Run `unzip -l iTLS-Enso.vpk` and assert the presence of `kernel.skprx`, `dnsfix.suprx`, `compat.suprx`, `lhttp.suprx`, `lssl.suprx`, `comm2.suprx`, and `certs.cer`.

- [ ] **Step 9: Commit build repairs**

```sh
git add Makefile .gitignore kernel/CMakeLists.txt old_dns_fix
git commit -m "build: restore current VitaSDK packaging"
```

### Task 6: Add CI and maintenance documentation

**Files:**
- Create: `.github/workflows/ci.yml`
- Create: `tools/update_ca_bundle.sh`
- Modify: `README.md`

- [ ] **Step 1: Add the host-validation job**

Install `build-essential`, `pkg-config`, and `libssl-dev`; run `make host-tests` and `make check-certs`.

- [ ] **Step 2: Add the VitaSDK job**

Use `vitasdk/vitasdk:latest`, run `make clean && make -j2`, and list the VPK payloads.

- [ ] **Step 3: Add the explicit certificate refresh wrapper**

Use:

```sh
curl -q -fsSL "$URL" -o "$temporary"
python3 tools/update_ca_bundle.py --sony ... --mozilla "$temporary" --output ...
```

Move the downloaded source into place only after generation and tests pass.

- [ ] **Step 4: Document installation semantics**

Explain explicit update/remove actions, the persistent original CA backup, current module origins, certificate source, `make check-certs`, `make update-certs`, and the fact that VPK reinstall alone does not modify `vs0:`.

- [ ] **Step 5: Commit CI and docs**

```sh
git add .github tools/update_ca_bundle.sh README.md
git commit -m "ci: validate CA loading and VitaSDK builds"
```

### Task 7: Final verification and publication

**Files:** all changed files

- [ ] **Step 1: Run fresh host verification**

```sh
make clean-host-tests
make host-tests
make check-certs
```

Expected: zero failures and loader output with equal parsed/added counts.

- [ ] **Step 2: Run fresh VitaSDK verification**

```sh
docker run --rm -v "$PWD:/src" -w /src vitasdk/vitasdk:latest sh -lc 'make clean && make -j2'
```

Expected: zero exit.

- [ ] **Step 3: Verify repository hygiene**

```sh
git diff --check
git status --short
git log --oneline origin/master..HEAD
```

Only intended source, test, documentation, generated certificate inputs/output, and workflow files may remain.

- [ ] **Step 4: Present the aggregate diff**

Call the connector's `show_diff` once after the last file change and inspect every hunk.

- [ ] **Step 5: Push the branch**

```sh
git push -u devnoname120 fix/ca-bundle-loading
```

- [ ] **Step 6: Open a superseding PR**

Use `gh pr create --repo SKGleba/iTLS-Enso --base master --head devnoname120:fix/ca-bundle-loading`. Explain that the regenerated bundle shifted and re-serialized the stock prefix, causing all 67 firmware-3.73 raw-PEM SHA-1 checks to fail; explain the independent duplicate-appended-root hazard, TLS/cipher findings, transactional installer behavior, build fixes, test evidence, and that the PR supersedes #37.
