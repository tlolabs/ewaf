#!/usr/bin/env python3
"""Generate the dependency-free Xcode UI-test harness from the SwiftPM sources."""
from pathlib import Path
import hashlib
import plistlib
from version import VERSION

root = Path(__file__).resolve().parent.parent
objects = {}
def oid(name):
    return hashlib.sha256(name.encode()).hexdigest()[:24].upper()
def add(key, isa, **values):
    identifier = oid(key)
    objects[identifier] = dict(isa=isa, **values)
    return identifier

def configurations(name, settings):
    configs = []
    for mode in ['Debug', 'Release']:
        config = dict(settings)
        config.update(ONLY_ACTIVE_ARCH='YES' if mode == 'Debug' else 'NO', SWIFT_OPTIMIZATION_LEVEL='-Onone' if mode == 'Debug' else '-O', DEBUG_INFORMATION_FORMAT='dwarf' if mode == 'Debug' else 'dwarf-with-dsym')
        configs.append(add(name+mode, 'XCBuildConfiguration', name=mode, buildSettings=config))
    return add(name+'configs', 'XCConfigurationList', buildConfigurations=configs, defaultConfigurationIsVisible='0', defaultConfigurationName='Release')

products = []
app_product = add('appProduct', 'PBXFileReference', explicitFileType='wrapper.application', path='EWAF.app', sourceTree='BUILT_PRODUCTS_DIR')
test_product = add('testProduct', 'PBXFileReference', explicitFileType='wrapper.cfbundle', path='EWAFUITests.xctest', sourceTree='BUILT_PRODUCTS_DIR')
product_group = add('products', 'PBXGroup', name='Products', children=[app_product, test_product], sourceTree='<group>')
refs=[]
def source_phase(name, paths):
    builds=[]
    for path in paths:
        relative='../../'+str(path.relative_to(root))
        ref=add(relative, 'PBXFileReference', lastKnownFileType='sourcecode.swift', path=relative, sourceTree='<group>')
        refs.append(ref)
        builds.append(add(relative+'build', 'PBXBuildFile', fileRef=ref))
    return add(name+'sources', 'PBXSourcesBuildPhase', buildActionMask='2147483647', files=builds, runOnlyForDeploymentPostprocessing='0')
app_sources=source_phase('app', sorted((root/'platform/macos/Sources/EWAF').rglob('*.swift')))
test_sources=source_phase('test', sorted((root/'tests/macos/EWAFUITests').glob('*.swift')))
local=add('package', 'XCLocalSwiftPackageReference', relativePath='../..')
core=add('core', 'XCSwiftPackageProductDependency', package=local, productName='EWAFCore')
core_build=add('coreBuild', 'PBXBuildFile', productRef=core)
frameworks=add('frameworks', 'PBXFrameworksBuildPhase', buildActionMask='2147483647', files=[core_build], runOnlyForDeploymentPostprocessing='0')
settings=dict(SWIFT_VERSION='6.0', MACOSX_DEPLOYMENT_TARGET='14.0', SDKROOT='macosx', CODE_SIGNING_ALLOWED='YES', CODE_SIGN_IDENTITY='-', CODE_SIGN_STYLE='Manual', GENERATE_INFOPLIST_FILE='YES', CURRENT_PROJECT_VERSION='2', MARKETING_VERSION=VERSION, PRODUCT_NAME='$(TARGET_NAME)', ENABLE_HARDENED_RUNTIME='YES')
icon_ref=add('icon', 'PBXFileReference', lastKnownFileType='folder.iconcomposer.icon', path='../../assets/icon/EWAF.icon', sourceTree='<group>')
refs.append(icon_ref)
icon_build=add('iconBuild', 'PBXBuildFile', fileRef=icon_ref)
resources=add('resources', 'PBXResourcesBuildPhase', buildActionMask='2147483647', files=[icon_build], runOnlyForDeploymentPostprocessing='0')
app_settings=dict(settings, ASSETCATALOG_COMPILER_APPICON_NAME='EWAF', PRODUCT_NAME='EWAF', PRODUCT_BUNDLE_IDENTIFIER='com.tlolabs.ewaf', INFOPLIST_KEY_CFBundleDisplayName='EWAF')
app_id=add('appTarget', 'PBXNativeTarget', name='EWAFMac', productName='EWAF', productReference=app_product, productType='com.apple.product-type.application', buildConfigurationList=configurations('app',app_settings), buildPhases=[app_sources,frameworks,resources], dependencies=[], buildRules=[], packageProductDependencies=[core])
dependency=add('testDependency', 'PBXTargetDependency', target=app_id)
test_settings=dict(settings, ENABLE_HARDENED_RUNTIME='NO', PRODUCT_BUNDLE_IDENTIFIER='com.tlolabs.ewaf.uitests', TEST_TARGET_NAME='EWAFMac', SWIFT_EMIT_LOC_STRINGS='NO')
test_id=add('testTarget', 'PBXNativeTarget', name='EWAFUITests', productName='EWAFUITests', productReference=test_product, productType='com.apple.product-type.bundle.ui-testing', buildConfigurationList=configurations('test',test_settings), buildPhases=[test_sources], dependencies=[dependency], buildRules=[])
main_group=add('mainGroup', 'PBXGroup', children=refs+[product_group], sourceTree='<group>')
project=add('project', 'PBXProject', attributes={'LastUpgradeCheck':'2600','TargetAttributes':{test_id:{'TestTargetID':app_id}}}, buildConfigurationList=configurations('project', {'CLANG_ENABLE_MODULES':'YES'}), compatibilityVersion='Xcode 14.0', developmentRegion='en', knownRegions=['en','Base'], mainGroup=main_group, productRefGroup=product_group, projectDirPath='', projectRoot='', targets=[app_id,test_id], packageReferences=[local])
folder=root/'platform/macos/EWAF.xcodeproj'
folder.mkdir(exist_ok=True)
with (folder/'project.pbxproj').open('wb') as output:
    plistlib.dump(dict(archiveVersion='1', classes={}, objectVersion='56', objects=objects, rootObject=project),output, sort_keys=False)
scheme=f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="2600" version="1.3">
<BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries>
<BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{app_id}" BuildableName="EWAF.app" BlueprintName="EWAFMac" ReferencedContainer="container:EWAF.xcodeproj"/></BuildActionEntry>
</BuildActionEntries></BuildAction>
<TestAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv="YES"><Testables><TestableReference skipped="NO"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{test_id}" BuildableName="EWAFUITests.xctest" BlueprintName="EWAFUITests" ReferencedContainer="container:EWAF.xcodeproj"/></TestableReference></Testables></TestAction>
<LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="YES" debugDocumentVersioning="YES" allowLocationSimulation="YES"><BuildableProductRunnable runnableDebuggingMode="0"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{app_id}" BuildableName="EWAF.app" BlueprintName="EWAFMac" ReferencedContainer="container:EWAF.xcodeproj"/></BuildableProductRunnable></LaunchAction>
<ProfileAction buildConfiguration="Release"/><AnalyzeAction buildConfiguration="Debug"/><ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/>
</Scheme>
'''
scheme_dir=folder/'xcshareddata/xcschemes'
scheme_dir.mkdir(parents=True,exist_ok=True)
(scheme_dir/'EWAF.xcscheme').write_text(scheme)
