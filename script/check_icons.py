#!/usr/bin/env python3
"""Validate committed native icon artwork without an image-tool dependency."""
from pathlib import Path
import hashlib
import json
import struct
import zlib
from generate_icons import composer_layers

ROOT = Path(__file__).resolve().parent.parent
ICONS = ROOT / 'assets/icon'


def png_size(data):
    assert data[:8] == b'\x89PNG\r\n\x1a\n', 'Missing PNG image'
    width, height = struct.unpack('>II', data[16:24])
    cursor = 8
    while cursor < len(data):
        length = struct.unpack('>I', data[cursor:cursor + 4])[0]
        chunk = data[cursor + 4:cursor + 8 + length]
        checksum = struct.unpack('>I', data[cursor + 8 + length:cursor + 12 + length])[0]
        assert zlib.crc32(chunk) == checksum, 'Corrupt PNG chunk'
        cursor += length + 12
    assert cursor == len(data)
    return width, height


def ico_images(data):
    reserved, kind, count = struct.unpack('<HHH', data[:6])
    assert (reserved, kind) == (0, 1)
    images = {}
    for i in range(count):
        w, h, _, _, planes, depth, size, offset = struct.unpack('<BBBBHHII', data[6 + 16*i:22 + 16*i])
        image = data[offset:offset + size]
        assert planes == 1 and depth == 32 and png_size(image) == (w or 256, h or 256)
        images[w or 256] = image
    assert set(images) == {16, 24, 32, 48, 64, 128, 256}
    return images


def check():
    composer = ROOT / 'assets/EWAF.icon'
    document = json.loads((composer / 'icon.json').read_text())
    layers = [layer for group in document['groups'] for layer in group['layers']]
    assert [layer['image-name'] for layer in layers] == [
        '03-Folder-Front.svg', '02-Calendar.svg', '01-Folder-Back.svg'
    ], 'Composer layers must render front, calendar, back in sidebar order'
    for name, expected in composer_layers().items():
        assert (composer / 'Assets' / name).read_bytes() == expected, f'Regenerate Composer geometry: {name}'
    manifest = json.loads((ICONS / 'manifest.json').read_text())
    for name, expected in manifest.items():
        assert hashlib.sha256((ICONS / name).read_bytes()).hexdigest() == expected, f'Regenerate icons: {name}'
    assert set(manifest) == {'ewaf.svg', 'ewaf.png', 'ewaf.icns', 'ewaf.ico'}
    assert png_size((ICONS / 'ewaf.png').read_bytes()) == (1024, 1024)
    ico_images((ICONS / 'ewaf.ico').read_bytes())
    data = (ICONS / 'ewaf.icns').read_bytes()
    assert data[:4] == b'icns' and struct.unpack('>I', data[4:8])[0] == len(data)
    entries = {}
    cursor = 8
    while cursor < len(data):
        kind, length = struct.unpack('>4sI', data[cursor:cursor + 8])
        assert length > 8
        entries[kind.decode()] = png_size(data[cursor + 8:cursor + length])
        cursor += length
    expected = {'icp4':16, 'icp5':32, 'icp6':64, 'ic07':128, 'ic08':256, 'ic09':512,
                'ic10':1024, 'ic11':32, 'ic12':64, 'ic13':256, 'ic14':512}
    assert entries == {key:(size,size) for key,size in expected.items()} and cursor == len(data)
    print('Validated Composer layers, SVG provenance, PNG integrity, seven ICO sizes and eleven ICNS representations')


if __name__ == '__main__':
    check()
