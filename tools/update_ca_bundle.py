#!/usr/bin/env python3

from __future__ import annotations

import argparse
import base64
import binascii
import dataclasses
import hashlib
import os
import pathlib
import re
import stat
import sys
import tempfile
from collections.abc import Sequence


BEGIN_MARKER = "-----BEGIN CERTIFICATE-----"
END_MARKER = "-----END CERTIFICATE-----"
PEM_BLOCK_RE = re.compile(
    rf"{re.escape(BEGIN_MARKER)}.*?{re.escape(END_MARKER)}",
    re.DOTALL,
)
MOZILLA_DATE_RE = re.compile(
    rb"^## Certificate data from Mozilla as of:\s*(.+?)\s*$",
    re.MULTILINE,
)


class BundleError(ValueError):
    pass


@dataclasses.dataclass(frozen=True)
class Certificate:
    source: str
    source_index: int
    pem: bytes
    der_sha256: str


@dataclasses.dataclass(frozen=True)
class SourceStats:
    name: str
    input_sha256: str
    certificate_date: str | None
    input_count: int
    output_count: int
    duplicate_count: int


@dataclasses.dataclass(frozen=True)
class BundleResult:
    certificates: tuple[Certificate, ...]
    sources: tuple[SourceStats, ...]
    duplicate_count: int


def _canonical_pem(der: bytes) -> bytes:
    encoded = base64.b64encode(der).decode("ascii")
    lines = [encoded[index : index + 64] for index in range(0, len(encoded), 64)]
    return (
        BEGIN_MARKER
        + "\n"
        + "\n".join(lines)
        + "\n"
        + END_MARKER
        + "\n"
    ).encode("ascii")


def _read_der_tlv(data: bytes, offset: int) -> tuple[int, int, int, int]:
    if offset >= len(data):
        raise ValueError("missing DER tag")

    tag = data[offset]
    offset += 1
    if tag & 0x1F == 0x1F:
        raise ValueError("unsupported high-tag-number DER value")
    if offset >= len(data):
        raise ValueError("missing DER length")

    first_length = data[offset]
    offset += 1
    if first_length < 0x80:
        value_length = first_length
    else:
        length_size = first_length & 0x7F
        if length_size == 0 or length_size > len(data) - offset:
            raise ValueError("invalid DER length")
        if data[offset] == 0:
            raise ValueError("non-minimal DER length")
        value_length = int.from_bytes(data[offset : offset + length_size], "big")
        if value_length < 0x80:
            raise ValueError("non-minimal DER length")
        offset += length_size

    value_end = offset + value_length
    if value_end > len(data):
        raise ValueError("truncated DER value")
    return tag, offset, value_end, value_end


def _validate_certificate_der(der: bytes) -> None:
    tag, certificate_start, certificate_end, next_offset = _read_der_tlv(der, 0)
    if tag != 0x30 or next_offset != len(der):
        raise ValueError("certificate is not one complete DER sequence")

    offset = certificate_start
    child_ranges: list[tuple[int, int]] = []
    for expected_tag in (0x30, 0x30, 0x03):
        child_tag, child_start, child_end, offset = _read_der_tlv(der, offset)
        if child_tag != expected_tag:
            raise ValueError("certificate has an invalid outer structure")
        child_ranges.append((child_start, child_end))

    if offset != certificate_end:
        raise ValueError("certificate has trailing outer fields")
    if child_ranges[0][0] == child_ranges[0][1]:
        raise ValueError("certificate has an empty TBSCertificate")
    if child_ranges[1][0] == child_ranges[1][1]:
        raise ValueError("certificate has an empty signature algorithm")

    signature_start, signature_end = child_ranges[2]
    if signature_end - signature_start < 2 or der[signature_start] > 7:
        raise ValueError("certificate has an invalid signature bit string")


def _decode_pem_certificate(block: str) -> bytes:
    lines = block.splitlines()
    if len(lines) < 3 or lines[0] != BEGIN_MARKER or lines[-1] != END_MARKER:
        raise ValueError("invalid PEM boundaries")

    encoded = "".join(lines[1:-1])
    try:
        der = base64.b64decode(encoded, validate=True)
    except (binascii.Error, ValueError) as error:
        raise ValueError("invalid PEM base64") from error
    if not der:
        raise ValueError("empty DER certificate")

    _validate_certificate_der(der)
    return der


def parse_certificates(source_name: str, data: bytes) -> tuple[Certificate, ...]:
    if not source_name:
        raise BundleError("source name must not be empty")

    try:
        text = data.decode("utf-8").replace("\r\n", "\n").replace("\r", "\n")
    except UnicodeDecodeError as error:
        raise BundleError(f"{source_name}: certificate source is not UTF-8") from error

    begin_count = text.count(BEGIN_MARKER)
    end_count = text.count(END_MARKER)
    if begin_count != end_count:
        raise BundleError(
            f"{source_name}: unbalanced certificate markers "
            f"({begin_count} begin, {end_count} end)"
        )

    matches = list(PEM_BLOCK_RE.finditer(text))
    if not matches:
        raise BundleError(f"{source_name}: no certificates found")
    if len(matches) != begin_count:
        raise BundleError(f"{source_name}: malformed certificate boundaries")

    certificates: list[Certificate] = []
    for source_index, match in enumerate(matches, start=1):
        block = match.group(0) + "\n"
        try:
            der = _decode_pem_certificate(block)
        except ValueError as error:
            raise BundleError(
                f"{source_name}: invalid certificate at index {source_index}"
            ) from error

        certificates.append(
            Certificate(
                source=source_name,
                source_index=source_index,
                pem=_canonical_pem(der),
                der_sha256=hashlib.sha256(der).hexdigest(),
            )
        )

    return tuple(certificates)


def build_bundle(sources: Sequence[tuple[str, bytes]]) -> BundleResult:
    if not sources:
        raise BundleError("at least one certificate source is required")

    seen: set[str] = set()
    output: list[Certificate] = []
    source_stats: list[SourceStats] = []

    for source_name, data in sources:
        parsed = parse_certificates(source_name, data)
        date_match = MOZILLA_DATE_RE.search(data)
        try:
            certificate_date = (
                date_match.group(1).decode("ascii")
                if date_match is not None
                else None
            )
        except UnicodeDecodeError as error:
            raise BundleError(
                f"{source_name}: certificate data date is not ASCII"
            ) from error
        included = 0
        duplicates = 0

        for certificate in parsed:
            if certificate.der_sha256 in seen:
                duplicates += 1
                continue
            seen.add(certificate.der_sha256)
            output.append(certificate)
            included += 1

        source_stats.append(
            SourceStats(
                name=source_name,
                input_sha256=hashlib.sha256(data).hexdigest(),
                certificate_date=certificate_date,
                input_count=len(parsed),
                output_count=included,
                duplicate_count=duplicates,
            )
        )

    if not output:
        raise BundleError("generated bundle contains no certificates")

    return BundleResult(
        certificates=tuple(output),
        sources=tuple(source_stats),
        duplicate_count=sum(stat.duplicate_count for stat in source_stats),
    )


def render_bundle(result: BundleResult) -> bytes:
    header = [
        "##",
        "## iTLS-Enso generated CA bundle",
        "## Generated by tools/update_ca_bundle.py; do not edit directly.",
        "##",
    ]

    for source in result.sources:
        header.extend(
            (
                f"## Source: {source.name}",
                f"##   Input SHA256: {source.input_sha256}",
            )
        )
        if source.certificate_date is not None:
            header.append(f"##   Certificate data date: {source.certificate_date}")
        header.extend(
            (
                f"##   Input certificates: {source.input_count}",
                f"##   Included certificates: {source.output_count}",
                f"##   Skipped duplicates: {source.duplicate_count}",
            )
        )

    header.extend(
        (
            f"## Output certificates: {len(result.certificates)}",
            f"## Total skipped duplicates: {result.duplicate_count}",
            "##",
            "",
            "",
        )
    )

    certificates = b"\n".join(
        certificate.pem.rstrip(b"\n") for certificate in result.certificates
    )
    return "\n".join(header).encode("ascii") + certificates + b"\n"


def generate_bundle_bytes(sources: Sequence[tuple[str, bytes]]) -> bytes:
    return render_bundle(build_bundle(sources))


def _write_atomic(path: pathlib.Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        output_mode = stat.S_IMODE(path.stat().st_mode)
    except FileNotFoundError:
        output_mode = 0o644
    file_descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
    )
    temporary_path = pathlib.Path(temporary_name)
    try:
        with os.fdopen(file_descriptor, "wb") as temporary_file:
            os.fchmod(temporary_file.fileno(), output_mode)
            temporary_file.write(data)
            temporary_file.flush()
            os.fsync(temporary_file.fileno())
        os.replace(temporary_path, path)
    except BaseException:
        temporary_path.unlink(missing_ok=True)
        raise


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate the Sony-first duplicate-free iTLS CA bundle."
    )
    parser.add_argument("--sony", type=pathlib.Path, required=True)
    parser.add_argument("--mozilla", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify that the output is current without modifying it",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _build_parser().parse_args(argv)

    try:
        sources = (
            ("Sony/Vita", arguments.sony.read_bytes()),
            ("Mozilla/curl", arguments.mozilla.read_bytes()),
        )
        result = build_bundle(sources)
        generated = render_bundle(result)

        if arguments.check:
            try:
                existing = arguments.output.read_bytes()
            except FileNotFoundError:
                print(f"out of date: {arguments.output} does not exist", file=sys.stderr)
                return 1
            if existing != generated:
                print(f"out of date: regenerate {arguments.output}", file=sys.stderr)
                return 1
        else:
            _write_atomic(arguments.output, generated)

        for source in result.sources:
            print(
                f"{source.name}: input={source.input_count} "
                f"included={source.output_count} duplicates={source.duplicate_count}"
            )
        print(
            f"output={len(result.certificates)} "
            f"duplicates={result.duplicate_count} path={arguments.output}"
        )
        return 0
    except (BundleError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
