param(
	[ValidateSet("Standard", "MCP")][string]$Variant = "Standard",
	[string]$ProjectPath,
	[string]$TargetName,
	[string]$Configuration = "Development",
	[string]$Platform = "",
	[string]$TestFilter = "PlaneToPBR",
	[string]$ReportOutputPath
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "PlaneToPBRVariants.ps1")

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
if (-not $ReportOutputPath) {
	$ReportOutputPath = if ($Variant -eq "Standard") { "Saved/Automation/PlaneToPBR" } else { "Saved/Automation/PlaneToPBRMCP" }
}
$reportOutput = Resolve-FullPath $ReportOutputPath

if (-not (Test-Path -LiteralPath $engineDir -PathType Container)) {
	throw "UE_ENGINE_DIR does not exist: $engineDir"
}

if ((Test-Path -LiteralPath (Join-Path $engineDir "Engine") -PathType Container) -and
	-not (Test-Path -LiteralPath (Join-Path $engineDir "Build") -PathType Container)) {
	$engineDir = Join-Path $engineDir "Engine"
}

Assert-PlaneToPBREngine -EngineDir $engineDir -Variant $Variant
if (-not $ProjectPath) {
	$repo = Split-Path -Parent $PSScriptRoot
	$hostRoot = Join-Path $repo "Artifacts/Variants/$Variant/TestHost"
	New-Item -ItemType Directory -Path $hostRoot -Force | Out-Null
	$pluginRoot = Join-Path $hostRoot "Plugins/PlaneToPBR"
	New-PlaneToPBRStage -PluginFile (Join-Path $repo "Plugins/PlaneToPBR/PlaneToPBR.uplugin") -Destination $pluginRoot -Variant $Variant
	$ProjectPath = Join-Path $hostRoot "TestHost.uproject"
	@{ FileVersion = 3; Plugins = @(@{ Name = "PlaneToPBR"; Enabled = $true }) } |
		ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $ProjectPath -Encoding UTF8
	New-Item -ItemType Directory -Path (Join-Path $hostRoot "Build") -Force | Out-Null
	Copy-Item -LiteralPath (Join-Path $PSScriptRoot "PackagePlaneToPBRPlugin.ps1") -Destination (Join-Path $hostRoot "Build") -Force
	Copy-Item -LiteralPath (Join-Path $PSScriptRoot "PlaneToPBRVariants.ps1") -Destination (Join-Path $hostRoot "Build") -Force
	if (-not $TargetName) { $TargetName = "UnrealEditor" }
} elseif (-not $TargetName) {
	$TargetName = [IO.Path]::GetFileNameWithoutExtension($ProjectPath) + "Editor"
}
$projectFile = Resolve-FullPath $ProjectPath
$pluginRoot = Join-Path (Split-Path -Parent $projectFile) "Plugins/PlaneToPBR"
Assert-PlaneToPBRVariant -Root $pluginRoot -Variant $Variant

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

$runStarted = Get-Date
& $editorCmd $projectFile "-ExecCmds=Automation RunTests $TestFilter" "-TestExit=Automation Test Queue Empty" "-ReportOutputPath=$reportOutput" -unattended -nop4 -nullrhi -nosplash
if ($LASTEXITCODE -ne 0) {
	throw "PlaneToPBR automation tests failed with exit code $LASTEXITCODE."
}

$reportFile = Join-Path $reportOutput "index.json"
if (-not (Test-Path -LiteralPath $reportFile) -or (Get-Item -LiteralPath $reportFile).LastWriteTime -lt $runStarted) {
	throw "Automation did not produce a fresh report: $reportFile"
}
$report = Get-Content -Raw -LiteralPath $reportFile | ConvertFrom-Json
if ($report.failed -gt 0 -or $report.notRun -gt 0 -or $report.inProcess -gt 0 -or ($report.succeeded + $report.succeededWithWarnings) -eq 0) {
	throw "Automation report is failed, incomplete, or empty."
}
$mcpTests = @($report.tests | Where-Object { $_.fullTestPath -like "PlaneToPBR.MCP.*" })
if ($Variant -eq "Standard" -and $mcpTests.Count -gt 0) { throw "Standard automation accidentally loaded MCP tests." }
if ($Variant -eq "MCP") {
	foreach ($name in @("ModuleLoad", "ToolsetRegistration", "InputValidation", "WorkflowInvocation")) {
		if (-not ($mcpTests | Where-Object { $_.fullTestPath -eq "PlaneToPBR.MCP.$name" -and $_.state -eq "Success" })) {
			throw "Required MCP test did not pass: $name"
		}
	}
}
Write-Host "PlaneToPBR $Variant automation tests completed: $($report.succeeded) passed."
