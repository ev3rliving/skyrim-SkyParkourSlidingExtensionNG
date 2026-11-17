param([string]$PortableToolsRoot = '', [string]$BuildDirectory = 'build-universal')
# Compatibility is now provided by the same DLL for every supported runtime.
& "$PSScriptRoot/Build.ps1" -PortableToolsRoot $PortableToolsRoot -BuildDirectory $BuildDirectory
