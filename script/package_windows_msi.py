#!/usr/bin/env python3
"""Create a Windows Installer transaction from the existing validated native stage."""
import argparse, hashlib, subprocess, uuid, xml.etree.ElementTree as X
from pathlib import Path
from version import ROOT, VERSION
parser=argparse.ArgumentParser()
parser.add_argument('--arch', choices=['x64','ARM64'], required=True)
parser.add_argument('--emit-only', action='store_true', help='Emit the WiX source for transaction qualification')
args=parser.parse_args()
# Windows Installer's documented numeric version bounds are stricter than SemVer.
major,minor,patch=map(int,VERSION.split('.'))
if major>255 or minor>255 or patch>65535:
    raise SystemExit('Version cannot be represented by MSI; requires an explicit migration decision')
ns='http://wixtoolset.org/schemas/v4/wxs'
X.register_namespace('',ns)
def add(parent,tag,**attrs): return X.SubElement(parent,'{'+ns+'}'+tag,attrs)
def identifier(value): return 'I'+hashlib.sha256(value.encode()).hexdigest()[:24]
wix=X.Element('{'+ns+'}Wix')
package=add(wix,'Package',Name='EWAF',Manufacturer='TLO Labs',Version=VERSION,
            UpgradeCode='{33C1F535-C25C-48C6-BDE6-F186ABFCB085}',Scope='perUser',Compressed='yes')
add(package,'MajorUpgrade',DowngradeErrorMessage='A newer version of EWAF is already installed.',Schedule='afterInstallInitialize')
add(package,'MediaTemplate',EmbedCab='yes')
add(package,'Property',Id='MSIRESTARTMANAGERCONTROL',Value='Disable')
local=add(package,'StandardDirectory',Id='LocalAppDataFolder')
lab=add(local,'Directory',Id='TLOLabsFolder',Name='TLO Labs')
install=add(lab,'Directory',Id='INSTALLFOLDER',Name='EWAF')
feature=add(package,'Feature',Id='Main',Title='EWAF',Level='1')
stage=ROOT/'dist'/('windows-'+args.arch)
directories={Path('.'):install}
for file in sorted(stage.rglob('*')):
    if not file.is_file() or file.name in ('Install.ps1','Uninstall.ps1','install-manifest.json'): continue
    relative=file.relative_to(stage)
    for parent in reversed(relative.parents):
        if parent not in directories:
            directories[parent]=add(directories[parent.parent],'Directory',Id=identifier('dir/'+parent.as_posix()),Name=parent.name)
    key=relative.as_posix();cid=identifier(key)
    component=add(directories[relative.parent],'Component',Id=cid,Guid=str(uuid.uuid5(uuid.NAMESPACE_URL,'com.tlolabs.ewaf/'+args.arch+'/'+key)),Bitness='always64')
    add(component,'File',Id=identifier('file/'+key),Source=str(file),Name=file.name)
    add(component,'RegistryValue',Root='HKCU',Key=r'Software\tlolabs\EWAF\Installer',Name=cid,Type='integer',Value='1',KeyPath='yes')
    add(feature,'ComponentRef',Id=cid)
programs=add(package,'StandardDirectory',Id='ProgramMenuFolder')
shortcut=add(programs,'Component',Id='Shortcut',Guid='{35C4A1DE-EDBB-4C18-86A5-87BC778C3043}')
add(shortcut,'Shortcut',Id='EWAFShortcut',Name='EWAF',Target='[INSTALLFOLDER]EWAF.exe',WorkingDirectory='INSTALLFOLDER')
add(shortcut,'RegistryValue',Root='HKCU',Key=r'Software\tlolabs\EWAF\Installer',Name='Shortcut',Type='integer',Value='1',KeyPath='yes')
add(feature,'ComponentRef',Id='Shortcut')
for directory_id in [lab, *directories.values()]:
    # RemoveFolder removes empty owned directories only; never removes user contents.
    add(shortcut,'RemoveFolder',Id=identifier('remove/'+directory_id.attrib['Id']),Directory=directory_id.attrib['Id'],On='uninstall')
source=ROOT/'build'/('ewaf-'+args.arch+'.wxs');source.parent.mkdir(exist_ok=True)
X.ElementTree(wix).write(source,encoding='utf-8',xml_declaration=True)
output=ROOT/'dist'/f'ewaf-{VERSION}-windows-{args.arch.lower()}.msi'
if not args.emit_only:
    subprocess.run(['wix','build','-arch',args.arch.lower(),str(source),'-o',str(output)],check=True)
print(output)
