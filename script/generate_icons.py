#!/usr/bin/env python3
"""Export native artwork from the SVG using librsvg; no application build dependency."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess
import tempfile
import copy
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
ICONS = ROOT / 'assets/icon'
SIZES = (16, 24, 32, 48, 64, 128, 256, 512, 1024)


def composer_layers():
    """Keep Composer geometry identical to the corresponding flat SVG shapes."""
    ET.register_namespace('', 'http://www.w3.org/2000/svg')
    source = ET.parse(ICONS / 'ewaf.svg').getroot()
    namespace = {'svg': 'http://www.w3.org/2000/svg'}
    layers = {}
    for name, identifier in [('01-Folder-Back', 'folder-back'),
                             ('02-Calendar', 'calendar'),
                             ('03-Folder-Front', 'folder-front')]:
        layer = ET.Element(source.tag, source.attrib)
        layer.append(copy.deepcopy(source.find('svg:defs', namespace)))
        shape = source.find(f".//*[@id='{identifier}']")
        assert shape is not None, f'Missing icon geometry: {identifier}'
        layer.append(copy.deepcopy(shape))
        layers[name + '.svg'] = ET.tostring(layer, encoding='utf-8') + b'\n'
    return layers


def generate():
    renderer = shutil.which('rsvg-convert')
    if not renderer:
        raise SystemExit('Install librsvg (rsvg-convert) to regenerate icon artwork.')
    with tempfile.TemporaryDirectory(prefix='ewaf-icons-') as temporary:
        images = {}
        for size in SIZES:
            output = Path(temporary) / f'{size}.png'
            subprocess.run([renderer, '-w', str(size), '-h', str(size), '-o', str(output),
                            str(ICONS / 'ewaf.svg')], check=True)
            images[size] = output.read_bytes()
        (ICONS / 'ewaf.png').write_bytes(images[1024])
        entries = [(b'icp4', 16), (b'icp5', 32), (b'icp6', 64), (b'ic07', 128),
                   (b'ic08', 256), (b'ic09', 512), (b'ic10', 1024), (b'ic11', 32),
                   (b'ic12', 64), (b'ic13', 256), (b'ic14', 512)]
        chunks = b''.join(kind + struct.pack('>I', 8 + len(images[size])) + images[size]
                          for kind, size in entries)
        (ICONS / 'ewaf.icns').write_bytes(b'icns' + struct.pack('>I', 8 + len(chunks)) + chunks)
        sizes = SIZES[:7]
        offset = 6 + 16 * len(sizes)
        directory = bytearray(struct.pack('<HHH', 0, 1, len(sizes)))
        for size in sizes:
            directory += struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32,
                                     len(images[size]), offset)
            offset += len(images[size])
        (ICONS / 'ewaf.ico').write_bytes(directory + b''.join(images[size] for size in sizes))
    for name, data in composer_layers().items():
        (ROOT / 'assets/EWAF.icon/Assets' / name).write_bytes(data)
    manifest = {name: hashlib.sha256((ICONS / name).read_bytes()).hexdigest()
                for name in ('ewaf.svg', 'ewaf.png', 'ewaf.icns', 'ewaf.ico')}
    (ICONS / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Generated PNG, ICNS and ICO artwork from assets/icon/ewaf.svg')


if __name__ == '__main__':
    generate()
