param(
	[string]$PluginPath = "Plugins/PlaneToPBR/PlaneToPBR.uplugin",
	[string]$PackageDir = "Artifacts/PlaneToPBR",
	[string]$ZipPath = "Artifacts/PlaneToPBR.zip",
	[string]$TargetPlatforms = $env:PLUGIN_TARGET_PLATFORMS
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
	param([string]$Path)

	if ([System.IO.Path]::IsPathRooted($Path)) {
		return [System.IO.Path]::GetFullPath($Path)
	}

	return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

if ([string]::IsNullOrWhiteSpace($env:UE_ENGINE_DIR)) {
	throw "UE_ENGINE_DIR must point to an installed Unreal Engine directory."
}

if ([string]::IsNullOrWhiteSpace($TargetPlatforms)) {
	$TargetPlatforms = "Win64"
}

$engineDir = Resolve-FullPath $env:UE_ENGINE_DIR
$pluginFile = Resolve-FullPath $PluginPath
$packageOutput = Resolve-FullPath $PackageDir
$zipOutput = Resolve-FullPath $ZipPath

if (-not (Test-Path -LiteralPath $engineDir -PathType Container)) {
	throw "UE_ENGINE_DIR does not exist: $engineDir"
}

if ((Test-Path -LiteralPath (Join-Path $engineDir "Engine") -PathType Container) -and
	-not (Test-Path -LiteralPath (Join-Path $engineDir "Build") -PathType Container)) {
	$engineDir = Join-Path $engineDir "Engine"
}

if (-not (Test-Path -LiteralPath $pluginFile -PathType Leaf)) {
	throw "Plugin file does not exist: $pluginFile"
}

$runUatCandidates = @(
	(Join-Path $engineDir "Build/BatchFiles/RunUAT.bat"),
	(Join-Path $engineDir "Build/BatchFiles/RunUAT.sh")
)

$runUat = $runUatCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $runUat) {
	throw "RunUAT was not found under UE_ENGINE_DIR: $engineDir"
}

$artifactRoot = Split-Path -Parent $packageOutput
if (-not (Test-Path -LiteralPath $artifactRoot)) {
	New-Item -ItemType Directory -Path $artifactRoot | Out-Null
}

if (Test-Path -LiteralPath $packageOutput) {
	Remove-Item -LiteralPath $packageOutput -Recurse -Force
}

if (Test-Path -LiteralPath $zipOutput) {
	Remove-Item -LiteralPath $zipOutput -Force
}

Write-Host "Packaging PlaneToPBR plugin"
Write-Host "UE_ENGINE_DIR: $engineDir"
Write-Host "Plugin: $pluginFile"
Write-Host "PackageDir: $packageOutput"
Write-Host "TargetPlatforms: $TargetPlatforms"

& $runUat BuildPlugin `
	"-Plugin=$pluginFile" `
	"-Package=$packageOutput" `
	"-TargetPlatforms=$TargetPlatforms"

if ($LASTEXITCODE -ne 0) {
	throw "RunUAT BuildPlugin failed with exit code $LASTEXITCODE."
}

$packagedPluginFile = Join-Path $packageOutput "PlaneToPBR.uplugin"
if (-not (Test-Path -LiteralPath $packagedPluginFile -PathType Leaf)) {
	throw "Packaged plugin descriptor was not found: $packagedPluginFile"
}

$hostProjectMarkers = @(
	"planetoPBR_unreal.uproject",
	"Source/planetoPBR_unreal",
	"Config/DefaultEngine.ini",
	"Content/Maps"
)

foreach ($marker in $hostProjectMarkers) {
	$markerPath = Join-Path $packageOutput $marker
	if (Test-Path -LiteralPath $markerPath) {
		throw "Package output includes host project content: $marker"
	}
}

$zipParent = Split-Path -Parent $zipOutput
if (-not (Test-Path -LiteralPath $zipParent)) {
	New-Item -ItemType Directory -Path $zipParent | Out-Null
}

Compress-Archive -Path (Join-Path $packageOutput "*") -DestinationPath $zipOutput -Force

Write-Host "Packaged plugin: $packageOutput"
Write-Host "Zipped artifact: $zipOutput"
