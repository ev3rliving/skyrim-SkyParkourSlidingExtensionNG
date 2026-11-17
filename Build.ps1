param(
    [string]$CMake = 'cmake',
    [string]$Generator = 'Ninja',
    [string]$BuildDirectory = 'build-universal',
    [string]$PortableToolsRoot = ''
)
$ErrorActionPreference = 'Stop'
if ($PortableToolsRoot) {
    $taskVC = Join-Path $PortableToolsRoot 'msvc/VC/Tools/MSVC/14.44.35207'
    $taskSDK = Join-Path $PortableToolsRoot 'sdk-extracted/Windows Kits/10'
    $taskCMakeBin = Join-Path $PortableToolsRoot 'tools/cmake-3.31.8-windows-x86_64/bin'
    $taskNinja = Join-Path $PortableToolsRoot 'tools/ninja'
    $env:PATH = "$taskVC/bin/Hostx64/x64;$taskSDK/bin/10.0.26100.0/x64;$taskCMakeBin;$taskNinja;$env:PATH"
    $env:INCLUDE = "$taskVC/include;$taskSDK/Include/10.0.26100.0/ucrt;$taskSDK/Include/10.0.26100.0/shared;$taskSDK/Include/10.0.26100.0/um;$taskSDK/Include/10.0.26100.0/winrt"
    $env:LIB = "$taskVC/lib/x64;$taskSDK/Lib/10.0.26100.0/ucrt/x64;$taskSDK/Lib/10.0.26100.0/um/x64"
}
$taskArguments = @('-S', $PSScriptRoot, '-B', "$PSScriptRoot/$BuildDirectory", '-G', $Generator)
if ($Generator -like 'Visual Studio*') { $taskArguments += @('-A', 'x64') }
else { $taskArguments += '-DCMAKE_BUILD_TYPE=Release' }
& $CMake @taskArguments
if ($LASTEXITCODE -ne 0) { throw 'Configuration failed.' }
& $CMake --build "$PSScriptRoot/$BuildDirectory" --config Release --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
& (Join-Path (Split-Path (Get-Command $CMake).Source) 'ctest.exe') --test-dir "$PSScriptRoot/$BuildDirectory" -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
& $CMake --install "$PSScriptRoot/$BuildDirectory" --config Release --component Slide --prefix "$PSScriptRoot/dist/universal"
if ($LASTEXITCODE -ne 0) { throw 'Staging failed.' }
Compress-Archive -Path "$PSScriptRoot/dist/universal/*" -DestinationPath "$PSScriptRoot/SkyParkourSlidingExtension-1.2.0.zip" -Force



