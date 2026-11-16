# Makes the Windows release zip from a build:
#
#   pwsh scripts/package-windows.ps1 [-BuildDir build] [-Config Release] [-OutDir dist]
#
# Installs the "release" component into <OutDir>/stage/<name>, checks that
# every DLL the programs import is in the folder or part of Windows, renders
# an idle clip with the staged engine-sim-export from another folder with a
# bare PATH, then writes <OutDir>/<name>.zip. Used by
# .github/workflows/release.yml; run it the same way locally.

param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [string]$OutDir = "dist"
)

$ErrorActionPreference = "Stop"

function Fail([string]$message) {
    Write-Host "::error::$message"
    throw $message
}

# The version CMake configured
$cache = Join-Path $BuildDir "CMakeCache.txt"
if (-not (Test-Path $cache)) { Fail "No CMakeCache.txt in '$BuildDir'; configure and build first" }
$match = Select-String -Path $cache -Pattern '^CMAKE_PROJECT_VERSION:STATIC=(.+)$'
if (-not $match) { Fail "CMAKE_PROJECT_VERSION is not in $cache" }
$version = $match.Matches[0].Groups[1].Value.Trim()

$name = "engine-sim-community-$version-windows-x64"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$stageRoot = Join-Path $OutDir "stage"
$stage = Join-Path $stageRoot $name
if (Test-Path $stageRoot) { Remove-Item -Recurse -Force $stageRoot }

# dumpbin finds the DLLs each program imports (cmake --install uses it too)
$dumpbin = (Get-Command dumpbin.exe -ErrorAction SilentlyContinue).Source
if (-not $dumpbin) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $dumpbin = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -find "VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe" |
            Sort-Object | Select-Object -Last 1
    }
}
if (-not $dumpbin) { Fail "dumpbin.exe not found; install the Visual Studio C++ tools" }
$env:PATH = (Split-Path $dumpbin) + ";" + $env:PATH

Write-Host "Installing $name"
cmake --install $BuildDir --config $Config --component release --prefix $stage
if ($LASTEXITCODE -ne 0) { Fail "cmake --install failed" }

# Every import must be in the folder or be part of Windows. The Visual C++
# runtime is in system32 on most machines too, but not on a fresh Windows,
# so only DLLs Microsoft ships as part of Windows count.
$programs = @(Get-ChildItem $stage -File | Where-Object { $_.Extension -in ".exe", ".dll" })
$present = @{}
foreach ($f in $programs) { $present[$f.Name.ToLowerInvariant()] = $true }
$problems = @()
foreach ($f in $programs) {
    $output = & $dumpbin /nologo /dependents $f.FullName
    if ($LASTEXITCODE -ne 0) { Fail "dumpbin failed on $($f.Name)" }
    foreach ($line in $output) {
        if ($line -notmatch '^\s+(\S+\.dll)\s*$') { continue }
        $dll = $Matches[1].ToLowerInvariant()
        if ($present.ContainsKey($dll) -or $dll -match '^(api|ext)-ms-') { continue }
        $system = Join-Path $env:SystemRoot "System32\$dll"
        if ((Test-Path $system) -and
            ((Get-Item $system).VersionInfo.ProductName -match 'Windows')) { continue }
        $problems += "$($f.Name) imports $dll, which is not in the package"
    }
}
if ($problems.Count -gt 0) { Fail ($problems -join "`n") }
Write-Host "All imports of $($programs.Count) programs and DLLs are in the package or in Windows"

# Smoke test: the staged exporter, started elsewhere, finds its scripts and
# libraries on its own
$smoke = Join-Path ([IO.Path]::GetTempPath()) ("engine-sim-smoke-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory $smoke | Out-Null
$savedPath = $env:PATH
$savedRoot = $env:ENGINE_SIM_DATA_ROOT
Push-Location $smoke
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:ENGINE_SIM_DATA_ROOT = $null
    & (Join-Path $stage "engine-sim-export.exe") --clips idle --out (Join-Path $smoke "out")
    if ($LASTEXITCODE -ne 0) { Fail "engine-sim-export failed in the staged folder (exit $LASTEXITCODE)" }
} finally {
    Pop-Location
    $env:PATH = $savedPath
    $env:ENGINE_SIM_DATA_ROOT = $savedRoot
}
$clips = @(Get-ChildItem (Join-Path $smoke "out") -Recurse -File |
    Where-Object { $_.Extension -in ".wav", ".ogg", ".mp3" -and $_.Length -gt 1000 })
$formats = ($clips | ForEach-Object { $_.Extension } | Sort-Object -Unique) -join " "
if (-not ($clips | Where-Object { $_.Extension -eq ".wav" })) { Fail "The smoke test wrote no WAV clip" }
Write-Host "Smoke test wrote $($clips.Count) clips ($formats)"
Remove-Item -Recurse -Force $smoke

# The zip holds one folder named like the zip
$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Push-Location $stageRoot
try {
    cmake -E tar cf $zip --format=zip $name
    if ($LASTEXITCODE -ne 0) { Fail "Could not write $zip" }
} finally {
    Pop-Location
}

$hash = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLowerInvariant()
Write-Host "$hash  $name.zip"

if ($env:GITHUB_OUTPUT) {
    "version=$version" | Out-File $env:GITHUB_OUTPUT -Append
    "zip=$zip" | Out-File $env:GITHUB_OUTPUT -Append
    "formats=$formats" | Out-File $env:GITHUB_OUTPUT -Append
}
