#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
temporary_directory=$(mktemp -d "${TMPDIR:-/tmp}/itls-cacert.XXXXXX")
trap 'rm -rf "$temporary_directory"' EXIT HUP INT TERM
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

mv "$mozilla_source" "$root/misc/mozilla_cacert.pem"
mv "$generated_bundle" "$root/misc/certs.cer"

python3 "$root/tools/update_ca_bundle.py" \
    --sony "$root/misc/sony_certs.cer" \
    --mozilla "$root/misc/mozilla_cacert.pem" \
    --output "$root/misc/certs.cer" \
    --check
