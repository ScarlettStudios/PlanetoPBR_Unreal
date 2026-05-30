param(
	[string]$PluginPath = "Plugins/PlaneToPBR/PlaneToPBR.uplugin",
	[string]$PackageDir = "Artifacts/PlaneToPBR",
	[string]$ZipPath = "Artifacts/PlaneToPBR.zip",
	[string]$TargetPlatforms = $env:PLUGIN_TARGET_PLATFORMS,
	[switch]$IncludeDebugSymbols,
	[switch]$IncludeTests
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
	param([string]$Path)

	if ([System.IO.Path]::IsPathRooted($Path)) {
		return [System.IO.Path]::GetFullPath($Path)
	}

	return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Remove-PackageEntries {
	param(
		[string]$Root,
		[scriptblock]$Predicate
	)

	Get-ChildItem -LiteralPath $Root -Recurse -Force |
		Where-Object { & $Predicate $_ $Root } |
		Sort-Object { $_.FullName.Length } -Descending |
		ForEach-Object {
			Write-Host "Removing release-only package entry: $($_.FullName.Substring($Root.Length + 1))"
			Remove-Item -LiteralPath $_.FullName -Recurse -Force
		}
}

function Remove-TestOutputsFromPrecompiledManifests {
	param([string]$Root)

	Get-ChildItem -LiteralPath $Root -Recurse -Force -File -Filter "*.precompiled" |
		ForEach-Object {
			$manifest = Get-Content -Raw -LiteralPath $_.FullName | ConvertFrom-Json
			if (-not $manifest.OutputFiles) {
				return
			}

			$outputFiles = @($manifest.OutputFiles)
			$releaseOutputFiles = @($outputFiles | Where-Object { $_ -notlike "*Tests.cpp.obj" })
			if ($releaseOutputFiles.Count -eq $outputFiles.Count) {
				return
			}

			$manifest.OutputFiles = $releaseOutputFiles
			$manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $_.FullName -Encoding UTF8
			Write-Host "Removed automation test object references from: $($_.FullName.Substring($Root.Length + 1))"
		}
}

# UE_ENGINE_DIR required to locate RunUAT.bat for plugin packaging automation
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

if (-not $IncludeTests) {
	Remove-TestOutputsFromPrecompiledManifests -Root $packageOutput
	Remove-PackageEntries -Root $packageOutput -Predicate {
		param($Entry, $Root)
		$relativePath = $Entry.FullName.Substring($Root.Length + 1)
		$relativePath -match '(^|\\)Tests(\\|$)' -or $Entry.Name -like '*Tests.*'
	}
}

if (-not $IncludeDebugSymbols) {
	Remove-PackageEntries -Root $packageOutput -Predicate {
		param($Entry, $Root)
		-not $Entry.PSIsContainer -and $Entry.Extension -eq ".pdb"
	}
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

$disallowedReleaseEntries = @(
	".git",
	".github",
	".idea",
	".vs",
	"Build",
	"Content",
	"DerivedDataCache",
	"Saved"
)

if (-not $IncludeTests) {
	$disallowedReleaseEntries += @(
		"Source/PlaneToPBR/Private/Tests",
		"Source/PlaneToPBREditor/Private/Tests"
	)
}

foreach ($entry in $disallowedReleaseEntries) {
	$entryPath = Join-Path $packageOutput $entry
	if (Test-Path -LiteralPath $entryPath) {
		throw "Package output includes a release-disallowed entry: $entry"
	}
}

if (-not $IncludeDebugSymbols) {
	$debugSymbols = Get-ChildItem -LiteralPath $packageOutput -Recurse -Force -File -Filter "*.pdb"
	if ($debugSymbols) {
		throw "Package output includes debug symbols. Use -IncludeDebugSymbols to keep them."
	}
}

if (-not $IncludeTests) {
	$testEntries = Get-ChildItem -LiteralPath $packageOutput -Recurse -Force |
		Where-Object {
			$relativePath = $_.FullName.Substring($packageOutput.Length + 1)
			$relativePath -match '(^|\\)Tests(\\|$)' -or $_.Name -like '*Tests.*'
		}

	if ($testEntries) {
		throw "Package output includes automation test files. Use -IncludeTests to keep them."
	}
}

$zipParent = Split-Path -Parent $zipOutput
if (-not (Test-Path -LiteralPath $zipParent)) {
	New-Item -ItemType Directory -Path $zipParent | Out-Null
}

Compress-Archive -Path $packageOutput -DestinationPath $zipOutput -Force

Write-Host "Packaged plugin: $packageOutput"
Write-Host "Zipped artifact: $zipOutput"
