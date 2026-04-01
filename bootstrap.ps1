[CmdletBinding()]
param(
    [ValidateSet("cuda", "cpu")]
    [string]$Mode = "cuda",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [string]$BuildDir = "",

    [bool]$InstallMissing = $true,
    [bool]$RunAfterBuild = $true,
    [bool]$Clean = $false
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Step([string]$Message) {
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Ensure-Winget() {
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        return
    }
    throw "winget is required to auto-install missing dependencies."
}

function Install-WingetPackage([string]$Id, [string]$DisplayName, [string]$Override = "") {
    Ensure-Winget
    Write-Step "Installing $DisplayName ($Id)"
    $args = @(
        "install",
        "--id", $Id,
        "-e",
        "--accept-package-agreements",
        "--accept-source-agreements"
    )
    if (-not [string]::IsNullOrWhiteSpace($Override)) {
        $args += @("--override", $Override)
    }

    & winget @args
    return ($LASTEXITCODE -eq 0)
}

function Ensure-Command([string]$CommandName, [string]$DisplayName, [string]$WingetId) {
    if (Get-Command $CommandName -ErrorAction SilentlyContinue) {
        return
    }

    if (-not $InstallMissing) {
        throw "$DisplayName is missing and InstallMissing is false."
    }

    if (-not (Install-WingetPackage -Id $WingetId -DisplayName $DisplayName)) {
        throw "Failed to install $DisplayName via winget ($WingetId)."
    }

    if (-not (Get-Command $CommandName -ErrorAction SilentlyContinue)) {
        throw "$DisplayName still not available after install."
    }
}

function Find-VcVars2022() {
    $candidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    )

    foreach ($path in $candidates) {
        if (Test-Path -LiteralPath $path) {
            return $path
        }
    }

    return $null
}

function Find-Nvcc() {
    $nvccCmd = Get-Command nvcc -ErrorAction SilentlyContinue
    if ($nvccCmd) {
        return $nvccCmd.Source
    }

    $defaultNvcc = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4\bin\nvcc.exe"
    if (Test-Path -LiteralPath $defaultNvcc) {
        return $defaultNvcc
    }

    return $null
}

function Ensure-Vs2022BuildTools() {
    $vcvars = Find-VcVars2022
    if ($vcvars) {
        return $vcvars
    }

    if (-not $InstallMissing) {
        throw "Visual Studio 2022 Build Tools not found and InstallMissing is false."
    }

    $override = "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --norestart"
    $ok = Install-WingetPackage -Id "Microsoft.VisualStudio.2022.BuildTools" -DisplayName "Visual Studio 2022 Build Tools" -Override $override
    if (-not $ok) {
        throw "Failed to install Visual Studio 2022 Build Tools."
    }

    $vcvars = Find-VcVars2022
    if (-not $vcvars) {
        throw "Visual Studio 2022 Build Tools install completed, but vcvars64.bat was not found."
    }

    return $vcvars
}

function Ensure-CudaToolkit() {
    $nvcc = Find-Nvcc
    if ($nvcc) {
        return $nvcc
    }

    if (-not $InstallMissing) {
        throw "CUDA Toolkit not found and InstallMissing is false."
    }

    $ids = @("NVIDIA.CUDA", "Nvidia.CUDA")
    foreach ($id in $ids) {
        if (Install-WingetPackage -Id $id -DisplayName "CUDA Toolkit") {
            break
        }
    }

    $nvcc = Find-Nvcc
    if (-not $nvcc) {
        throw "CUDA Toolkit install did not provide nvcc.exe. Install CUDA manually, then re-run."
    }

    return $nvcc
}

$projectRoot = Split-Path -Parent $PSCommandPath
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $env:TEMP ("cppsim_{0}_nmake22" -f $Mode)
}

Write-Step "Project root: $projectRoot"
Write-Step "Build mode: $Mode ($Config)"
Write-Step "Build directory: $BuildDir"

Ensure-Command -CommandName "cmake" -DisplayName "CMake" -WingetId "Kitware.CMake"
Ensure-Command -CommandName "git" -DisplayName "Git" -WingetId "Git.Git"

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
    Write-Step "Cleaning build directory"
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}

if ($Mode -eq "cuda") {
    $vcvars = Ensure-Vs2022BuildTools
    $nvcc = Ensure-CudaToolkit

    $configureCmd = "cmake -S `"$projectRoot`" -B `"$BuildDir`" -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=$Config -DCMAKE_CUDA_COMPILER=`"$nvcc`""
    $buildCmd = "cmake --build `"$BuildDir`""
    $exePath = Join-Path $BuildDir "SimulationEngine.exe"
    $runCmd = if ($RunAfterBuild) { "`"$exePath`"" } else { "echo Build complete: $exePath" }

    $cmdChain = "call `"$vcvars`" && $configureCmd && $buildCmd && $runCmd"
    Write-Step "Running configure/build pipeline through vcvars64.bat"
    & cmd.exe /c $cmdChain
    if ($LASTEXITCODE -ne 0) {
        throw "CUDA build pipeline failed with exit code $LASTEXITCODE."
    }
} else {
    Write-Step "Configuring CPU-only build"
    & cmake -S $projectRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed for CPU build."
    }

    Write-Step "Building CPU-only target"
    & cmake --build $BuildDir --config $Config --target SimulationEngine
    if ($LASTEXITCODE -ne 0) {
        throw "CPU build failed."
    }

    if ($RunAfterBuild) {
        $exePath = Join-Path $BuildDir (Join-Path $Config "SimulationEngine.exe")
        Write-Step "Launching executable"
        & $exePath
    } else {
        Write-Step "Build complete"
    }
}

Write-Step "Done"
