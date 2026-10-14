#!/usr/bin/env python3
"""Inventory an extracted local package without executing its binaries.

Source declarations, observed bytes, and missing provenance remain distinct.
Extraction is deliberately explicit: this tool never executes an AppImage/MSI.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import struct
import subprocess


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def pe_version(data):
    """Read VS_FIXEDFILEINFO only from the PE's RT_VERSION resource tree."""
    try:
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        sections, optional_size = struct.unpack_from('<H12xH', data, pe + 6)
        optional = pe + 24
        magic = struct.unpack_from('<H', data, optional)[0]
        directories = optional + (112 if magic == 0x20b else 96)
        resource_rva = struct.unpack_from('<I', data, directories + 16)[0]
        table = optional + optional_size
        def offset(rva):
            for i in range(sections):
                size, address, raw_size, raw = struct.unpack_from('<IIII', data, table + i * 40 + 8)
                if address <= rva < address + min(size, raw_size):
                    return raw + rva - address
            raise ValueError('RVA outside file-backed section')
        base = offset(resource_rva)
        def entries(relative):
            location = base + relative
            named, numbered = struct.unpack_from('<HH', data, location + 12)
            if named + numbered > 4096:
                raise ValueError('Oversized resource directory')
            return [struct.unpack_from('<II', data, location + 16 + 8 * i) for i in range(named + numbered)]
        versions = set()
        def visit(relative, depth):
            if depth > 3:
                raise ValueError('Resource directory too deep')
            for _, child in entries(relative):
                if child & 0x80000000:
                    visit(child & 0x7fffffff, depth + 1)
                else:
                    rva, size = struct.unpack_from('<II', data, base + child)
                    location = offset(rva)
                    value = data[location:location + size]
                    key = 'VS_VERSION_INFO\0'.encode('utf-16le')
                    if value[6:6 + len(key)] != key:
                        continue
                    fixed = (6 + len(key) + 3) & ~3
                    signature, version, high, low = struct.unpack_from('<IIII', value, fixed)
                    if signature == 0xfeef04bd and version == 0x10000:
                        versions.add(f'{high >> 16}.{high & 65535}.{low >> 16}.{low & 65535}')
        for resource_type, child in entries(0):
            if resource_type == 16 and child & 0x80000000:
                visit(child & 0x7fffffff, 1)
        return sorted(versions)
    except (struct.error, ValueError, IndexError):
        return []


def binary_metadata(path):
    # No binary is loaded or executed. OS inspection utilities only read it.
    with path.open('rb') as stream:
        header = stream.read(64)
    result = {}
    tool = None
    if header.startswith(b'MZ'):
        data = path.read_bytes()
        try:
            location = struct.unpack_from('<I', data, 0x3c)[0]
            if data[location:location + 4] != b'PE\0\0':
                return {}
            cpu = struct.unpack_from('<H', data, location + 4)[0]
            result = {'format': 'PE', 'architecture': {0x8664: 'x64', 0xaa64: 'arm64', 0x14c: 'x86'}.get(cpu, hex(cpu)),
                      'file_versions': pe_version(data), 'version_evidence': 'PE RT_VERSION; empty means unavailable'}
            if shutil.which('llvm-readobj'):
                tool = ['llvm-readobj', '--coff-imports', str(path)]
        except struct.error:
            return {'format': 'malformed PE'}
    elif header.startswith(b'\x7fELF') and len(header) >= 20:
        endian = '<' if header[5] == 1 else '>'
        cpu = struct.unpack_from(endian + 'H', header, 18)[0]
        result = {'format': 'ELF', 'architecture': {62: 'x64', 183: 'arm64'}.get(cpu, str(cpu)),
                  'version_evidence': 'unverified; SONAME/filename is not distribution patch provenance'}
        if shutil.which('readelf'):
            tool = ['readelf', '-d', str(path)]
    elif header[:4] in (b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
        cpu_names = {0x01000007: 'x64', 0x0100000c: 'arm64'}
        try:
            if header[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
                with path.open('rb') as stream:
                    fat = stream.read(8 + 64 * 32)
                count = struct.unpack_from('>I', fat, 4)[0]
                if count > 64:
                    raise ValueError('Too many Mach-O slices')
                stride = 32 if header[:4] == b'\xca\xfe\xba\xbf' else 20
                cpus = [struct.unpack_from('>I', fat, 8 + stride * index)[0] for index in range(count)]
            else:
                cpus = [struct.unpack_from('<I', header, 4)[0]]
            architectures = [cpu_names.get(cpu, hex(cpu)) for cpu in cpus]
        except (struct.error, ValueError):
            architectures = []
        result = {'format': 'Mach-O', 'architectures': architectures}
        if shutil.which('otool'):
            tool = ['otool', '-hvL', str(path)]
    if tool:
        process = subprocess.run(tool, capture_output=True, text=True, timeout=30)
        result['load_commands'] = process.stdout
        result['inspection_exit_code'] = process.returncode
        if process.returncode:
            result['inspection_error'] = process.stderr
    elif result:
        result['dependency_inspection'] = 'unavailable on this host; pending native inspection'
    return result


def inspect_tree(directory, platform, architecture, package=None, runtime=None):
    root = directory.resolve(strict=True)
    if not root.is_dir():
        raise ValueError('Expected an extracted package directory')
    files = []
    for parent, directories, names in os.walk(root, followlinks=False):
        # Record directory symlinks too, but never traverse them.
        links = [name for name in directories if (Path(parent) / name).is_symlink()]
        directories[:] = [name for name in directories if name not in links]
        for name in sorted(names + links):
            path = Path(parent) / name
            entry = {'path': path.relative_to(root).as_posix()}
            if path.is_symlink():
                entry.update({'kind': 'symlink', 'target': os.readlink(path),
                              'contained': path.resolve().is_relative_to(root)})
                if not entry['contained']:
                    raise ValueError(f'Package symlink escapes extracted tree: {entry["path"]}')
            elif path.is_file():
                entry.update({'kind': 'file', 'size': path.stat().st_size, 'sha256': sha256(path)})
                metadata = binary_metadata(path)
                if metadata:
                    entry['binary'] = metadata
                    entry['license_review'] = 'pending; reconcile with included notices and upstream source'
                if path.name == 'Info.plist':
                    with path.open('rb') as stream:
                        info = plistlib.load(stream)
                    entry['bundle'] = {key: info[key] for key in ('CFBundleIdentifier', 'CFBundleVersion', 'CFBundleShortVersionString') if key in info}
            else:
                raise ValueError(f'Unsupported special file: {path}')
            files.append(entry)
    result = {'schema_version': 1, 'evidence_kind': 'observed_extracted_tree',
              'platform': platform, 'architecture_requested': architecture,
              'provenance_status': 'unverified; file hashes do not establish vendor patches, licenses or package/tree correspondence',
              'files': sorted(files, key=lambda entry: entry['path'])}
    if package:
        result['package'] = {'filename': package.name, 'size': package.stat().st_size, 'sha256': sha256(package)}
    if runtime:
        if not package:
            raise ValueError('--runtime-file requires --package-file')
        with package.open('rb') as stream:
            prefix_matches = stream.read(runtime.stat().st_size) == runtime.read_bytes()
        pins = json.loads((Path(__file__).resolve().parent.parent / 'updates/build-tools.json').read_text())
        pin_arch = {'x64': 'x86_64', 'arm64': 'aarch64'}[architecture]
        result['appimage_runtime'] = {'sha256': sha256(runtime), 'matches_package_prefix': prefix_matches,
                                     'matches_declared_pin': sha256(runtime) == pins[pin_arch]['runtime'][1]}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tree', type=Path, required=True)
    parser.add_argument('--platform', choices=['macos', 'windows', 'linux'], required=True)
    parser.add_argument('--arch', choices=['x64', 'arm64'], required=True)
    parser.add_argument('--package-file', type=Path)
    parser.add_argument('--runtime-file', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = inspect_tree(args.tree, args.platform, args.arch, args.package_file, args.runtime_file)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n')
    print(f'Observed {len(result["files"])} entries; vendor patch and license review remain pending')


if __name__ == '__main__':
    main()
