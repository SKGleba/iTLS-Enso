# iTLS-Enso

Adds TLS 1.2 support to PlayStation Vita systems by loading newer system TLS modules and an updated CA bundle.

## Install or update

1. Install the VPK.
2. Start iTLS-Enso.
3. Select **Install/update the full iTLS package**.
4. Reboot when prompted.

The menu also exposes explicit component-level install/update, remove, and CA restore actions. An action no longer changes from install to uninstall based on hidden filesystem state.

Reinstalling the VPK updates only the files under `ux0:app`. It does **not** update the active files under `vs0:` or `ur0:`. Start the application and select the relevant install/update action after installing a newer VPK.

The first CA installation preserves the original system bundle as:

```text
vs0:/data/external/cert/CA_LIST.cer_old
```

Later CA updates replace only the active iTLS bundle. **Restore the original CA certificate bundle** copies the backup back into service and removes the backup after a successful restore.

## Component actions

The application provides explicit actions for:

- the complete package;
- the kernel module-redirection plugin;
- the browser compatibility module;
- the in-app HTTP/SSL modules;
- the CA certificate bundle;
- restoring or removing each installed component.

Every copy is written to a temporary file and size-checked before activation. Existing files are retained as rollback files until activation succeeds. A failed mount, read, write, sync, close, rename, or verification operation is shown with its stage and Vita error code; failed operations do not enter the success/reboot path.

## Building

A current VitaSDK installation is required.

```sh
make clean
make -j2
```

The top-level build compiles the application, kernel plugin, and 3.60 DNS compatibility plugin before packaging `iTLS-Enso.vpk`.

Host validation requires Python 3.10 or newer, a C compiler, `pkg-config`, and OpenSSL development headers:

```sh
make test
```

The host tests cover CA generation, Vita-compatible CA ingestion semantics, kernel module-path selection, transactional file replacement, and taiHEN configuration rewriting.

## CA bundle maintenance

The generated bundle has two pinned inputs:

```text
misc/sony_certs.cer
misc/mozilla_cacert.pem
```

`misc/sony_certs.cer` contains the preserved Sony/Vita roots. `misc/mozilla_cacert.pem` contains the pinned Mozilla extract distributed by curl. `tools/update_ca_bundle.py` preserves Sony ordering, appends Mozilla roots in Mozilla order, and removes later certificates with the same DER SHA-256 fingerprint.

Validate the committed output without network access:

```sh
make check-certs
```

Refresh the pinned Mozilla source and regenerate the bundle:

```sh
make update-certs
```

The refresh script downloads from curl with `curl -q`, validates the candidate bundle with the host loader test, then replaces the pinned source and generated output. Normal builds and CI do not download certificates.

## FAQ

### Can I use it without Enso?

The installer and CA-bundle action can run without Enso. Persistent browser and app-module replacement requires the kernel redirection plugin to load during boot, so those replacements require Enso or an equivalent persistent taiHEN setup.

### Where is the active CA bundle?

```text
vs0:/data/external/cert/CA_LIST.cer
```

### Can I delete the bubble after installation?

Yes. The installed modules and CA bundle are independent of the installer application. Keep the VPK available if you expect to update or remove components later.

## Troubleshooting

Reinstalling the VPK is not evidence that the active files were updated. Export the installed files with VitaShell and compare their SHA-256 hashes with the corresponding VPK payloads:

| VPK payload | Installed path |
|---|---|
| `kernel.skprx` | `ur0:tai/itls.skprx` |
| `compat.suprx` | `vs0:/data/external/itls/ScePsp2Compat.suprx` |
| `lhttp.suprx` | `vs0:/data/external/itls/libhttp.suprx` |
| `lssl.suprx` | `vs0:/data/external/itls/libssl.suprx` |
| `certs.cer` | `vs0:/data/external/cert/CA_LIST.cer` |

The kernel log reports the detected firmware, available replacement sets, and every module path redirect. A working browser replacement should produce messages resembling:

```text
itls module loaded
itls firmware=0x03600000 replacements: browser=1 app_tls=1 commerce=1
itls redirect: .../webcore/ScePsp2Compat.suprx -> vs0:/data/external/itls/ScePsp2Compat.suprx
```

For an unresolved HTTPS failure, capture the Vita's TLS ClientHello at the access point. TLS 1.0 or 1.1 indicates that the browser replacement is not active. TLS 1.2 without an expected ECDHE-ECDSA AES-GCM suite indicates that a different TLS module is running. A client alert after the server certificate shifts the investigation to certificate ingestion, the system clock, or TLS interception by the network. Retest with a correct clock and a non-intercepting network before changing the CA bundle again.

## Notes

- Since version 3.0, iTLS also replaces app-side TLS modules.
- Since version 3.2, iTLS patches the NP server hostname on firmware 3.60.
- Since version 3.2.1, iTLS includes external CA roots because Sony stopped updating the system bundle.
- Version 3.3 makes CA generation duplicate-free, adds loader-accurate validation, and makes installation/update/removal explicit and transactional.
- Version 3.3 also removes obsolete direct `.supr0` overrides and the legacy `ur0:tai/itls.suprx` entry during migration.
- Current replacement modules originate from firmware 3.73.
- The preserved Sony CA prefix originates from firmware 3.74-era system data; external roots come from the pinned Mozilla/curl bundle.

## Credits

Thanks to:

- Team Molecule
- Testers from the HenKaku Discord server
- Cuevavirus for the 3.60 NP server fix
- CreepNT for the `vshIoMount` fix
- olokos for the external CA bundle work
