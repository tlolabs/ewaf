#!/usr/bin/env python3
"""Check committed application version copies against the Rust workspace."""
import plistlib
import tomllib

from version import ROOT, VERSION


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def check_versions():
    cmake_text = (ROOT / 'CMakeLists.txt').read_text()
    require(f'project(EWAF VERSION {VERSION} ' in cmake_text,
            'CMakeLists.txt version must match Cargo.toml')

    lock_packages = tomllib.loads((ROOT / 'Cargo.lock').read_text())['package']
    for path in (ROOT / 'crates').glob('*/Cargo.toml'):
        if path.parent.name == 'tlo-updater':
            continue  # Independently versioned reusable library.
        package = tomllib.loads(path.read_text())['package']
        require(package.get('version') == {'workspace': True},
                f'{path.relative_to(ROOT)} must inherit the workspace version')
        local_versions = [entry['version'] for entry in lock_packages
                          if entry['name'] == package['name'] and 'source' not in entry]
        require(local_versions == [VERSION],
                f'Cargo.lock version for {package["name"]} must match Cargo.toml')

    project_path = ROOT / 'platform/macos/EWAF.xcodeproj/project.pbxproj'
    objects = plistlib.loads(project_path.read_bytes())['objects']
    targets = [value for value in objects.values() if value.get('isa') == 'PBXNativeTarget']
    require(targets, 'Xcode project must contain native targets')
    for target in targets:
        configurations = objects[target['buildConfigurationList']]['buildConfigurations']
        require(configurations, f'Xcode target {target["name"]} must contain build configurations')
        for identifier in configurations:
            configuration = objects[identifier]
            settings = configuration.get('buildSettings', {})
            for key in ('CURRENT_PROJECT_VERSION', 'MARKETING_VERSION'):
                require(settings.get(key) == VERSION,
                        f'Xcode {target["name"]}/{configuration["name"]} {key} must match Cargo.toml')
    print('Unified version: ' + VERSION)


if __name__ == '__main__':
    check_versions()
