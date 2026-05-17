param(
	[string]$ProjectPath = "planetoPBR_unreal.uproject",
	[string]$TargetName = "planetoPBR_unrealEditor",
	[string]$Configuration = "Development",
	[string]$Platform = "",
	[string]$TestFilter = "PlaneToPBR",
	[string]$ReportOutputPath = "Saved/Automation/PlaneToPBR"
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
	param([string]$Path)

	if ([System.IO.Path]::IsPathRooted($Path)) {
		return [System.IO.Path]::GetFullPath($Path)
	}

	return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Test-IsWindowsHost {
	if (Get-Variable -Name IsWindows -Scope Global -ErrorAction SilentlyContinue) {
		return $IsWindows
	}

	return [System.Environment]::OSVersion.Platform -eq [System.PlatformID]::Win32NT
}

# UE_ENGINE_DIR required to locate Build.bat and UnrealEditor-Cmd.exe for test execution
if ([string]::IsNullOrWhiteSpace($env:UE_ENGINE_DIR)) {
	throw "UE_ENGINE_DIR must point to an installed Unreal Engine directory."
}

if ([string]::IsNullOrWhiteSpace($Platform)) {
	$Platform = if (Test-IsWindowsHost) { "Win64" } else { "Linux" }
}

$engineDir = Resolve-FullPath $env:UE_ENGINE_DIR
$projectFile = Resolve-FullPath $ProjectPath
$reportOutput = Resolve-FullPath $ReportOutputPath

if (-not (Test-Path -LiteralPath $engineDir -PathType Container)) {
	throw "UE_ENGINE_DIR does not exist: $engineDir"
}

if ((Test-Path -LiteralPath (Join-Path $engineDir "Engine") -PathType Container) -and
	-not (Test-Path -LiteralPath (Join-Path $engineDir "Build") -PathType Container)) {
	$engineDir = Join-Path $engineDir "Engine"
}

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
	throw "Project file does not exist: $projectFile"
}

$buildCandidates = @(
	(Join-Path $engineDir "Build/BatchFiles/Build.bat"),
	(Join-Path $engineDir "Build/BatchFiles/Linux/Build.sh"),
	(Join-Path $engineDir "Build/BatchFiles/Build.sh")
)

$buildTool = $buildCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $buildTool) {
	throw "Unreal build script was not found under UE_ENGINE_DIR: $engineDir"
}

$editorCmdCandidates = @(
	(Join-Path $engineDir "Binaries/Win64/UnrealEditor-Cmd.exe"),
	(Join-Path $engineDir "Binaries/Linux/UnrealEditor-Cmd")
)

$editorCmd = $editorCmdCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $editorCmd) {
	throw "UnrealEditor-Cmd was not found under UE_ENGINE_DIR: $engineDir"
}

Write-Host "Building PlaneToPBR automation test target"
Write-Host "UE_ENGINE_DIR: $engineDir"
Write-Host "Project: $projectFile"
Write-Host "Target: $TargetName"
Write-Host "Platform: $Platform"
Write-Host "Configuration: $Configuration"

& $buildTool $TargetName $Platform $Configuration "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) {
	throw "Unreal editor target build failed with exit code $LASTEXITCODE."
}

Write-Host "Running PlaneToPBR automation tests"
Write-Host "TestFilter: $TestFilter"
Write-Host "ReportOutputPath: $reportOutput"

& $editorCmd $projectFile "-ExecCmds=Automation RunTests $TestFilter" "-TestExit=Automation Test Queue Empty" "-ReportOutputPath=$reportOutput" -unattended -nop4 -nullrhi -nosplash
if ($LASTEXITCODE -ne 0) {
	throw "PlaneToPBR automation tests failed with exit code $LASTEXITCODE."
}

Write-Host "PlaneToPBR automation tests completed."
