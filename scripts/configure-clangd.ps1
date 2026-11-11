$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found at $vswhere"
}

$vsInstall = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $vsInstall) {
    throw 'No Visual Studio installation with the C++ toolset was found.'
}

$vcvars = Join-Path $vsInstall 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat not found at $vcvars"
}

Push-Location $root
try {
    & cmd.exe /c "`"$vcvars`" >nul && cmake --preset clangd"
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}

# powershell -NoProfile -ExecutionPolicy Bypass -File scripts\configure-clangd.ps1