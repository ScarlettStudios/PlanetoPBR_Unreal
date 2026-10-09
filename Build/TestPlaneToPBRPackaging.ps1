$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "PlaneToPBRVariants.ps1")

$repoRoot = Split-Path -Parent $PSScriptRoot
$pluginFile = Join-Path $repoRoot "Plugins/PlaneToPBR/PlaneToPBR.uplugin"
$artifactsRoot = Join-Path $repoRoot "Artifacts"
$engineDir = Join-Path $repoRoot "Build"
$testRoot = Join-Path $artifactsRoot "Variants/PackagingGuardTests-$([Guid]::NewGuid().ToString('N'))"

if (-not (Test-PlaneToPBRPathOverlap (Join-Path $testRoot "Output") (Join-Path $testRoot "Output/Child"))) {
	throw "Packaging safeguards failed to detect nested output paths."
}

try {
	Assert-PlaneToPBROutputPaths -Variant Standard -PluginFile $pluginFile -EngineDir $engineDir -Paths @(
		(Join-Path $testRoot "Output"),
		(Join-Path $testRoot "Output/Child"))
	throw "Packaging safeguards accepted overlapping output paths."
}
catch {
	if ($_.Exception.Message -notlike "Selected variant output paths overlap:*") {
		throw
	}
}

try {
	Assert-PlaneToPBROutputPaths -Variant Standard -PluginFile $pluginFile -EngineDir $engineDir -Paths @(
		(Join-Path $artifactsRoot "Variants/MCP"))
	throw "Packaging safeguards accepted the other variant's output path."
}
catch {
	if ($_.Exception.Message -notlike "Output path collides with protected source, engine, or variant output:*") {
		throw
	}
}

& (Join-Path $PSScriptRoot "TestPlaneToPBREngine.ps1")
Write-Host "PlaneToPBR packaging isolation safeguards passed."