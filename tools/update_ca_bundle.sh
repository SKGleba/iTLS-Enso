#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
temporary_directory=$(mktemp -d "${TMPDIR:-/tmp}/itls-cacert.XXXXXX")
mozilla_stage="$root/misc/.mozilla_cacert.pem.itls-new.$$"
bundle_stage="$root/misc/.certs.cer.itls-new.$$"

cleanup() {
    rm -rf "$temporary_directory"
    rm -f "$mozilla_stage" "$bundle_stage"
}

trap cleanup EXIT HUP INT TERM
cd "$root"

mozilla_source="$temporary_directory/mozilla_cacert.pem"
generated_bundle="$temporary_directory/certs.cer"

curl -q -fsSL https://curl.se/ca/cacert.pem -o "$mozilla_source"
python3 "$root/tools/update_ca_bundle.py" \
    --sony "$root/misc/sony_certs.cer" \
    --mozilla "$mozilla_source" \
    --output "$generated_bundle"

make -C "$root" build/host-tests/ca_loader_test
"$root/build/host-tests/ca_loader_test" "$generated_bundle"
ITLS_MOZILLA_SOURCE="$mozilla_source" \
ITLS_CA_BUNDLE="$generated_bundle" \
    python3 -m unittest -v tests.test_update_ca_bundle

rm -f "$mozilla_stage" "$bundle_stage"
cp "$mozilla_source" "$mozilla_stage"
cp "$generated_bundle" "$bundle_stage"
chmod 0644 "$mozilla_stage" "$bundle_stage"
mv "$mozilla_stage" "$root/misc/mozilla_cacert.pem"
mv "$bundle_stage" "$root/misc/certs.cer"

python3 "$root/tools/update_ca_bundle.py" \
    --sony "$root/misc/sony_certs.cer" \
    --mozilla "$root/misc/mozilla_cacert.pem" \
    --output "$root/misc/certs.cer" \
    --check
