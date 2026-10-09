$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "PlaneToPBRVariants.ps1")

$repoRoot = Split-Path -Parent $PSScriptRoot
$testRoot = Join-Path $repoRoot "Artifacts/Variants/EngineGuardTests-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path (Join-Path $testRoot "Build") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $testRoot "Plugins") -Force | Out-Null

try {
	foreach ($variant in @("Standard", "MCP")) {
		try {
			Assert-PlaneToPBREngine -EngineDir $testRoot -Variant $variant
			throw "$variant accepted an engine without Build.version."
		} catch {
			if ($_.Exception.Message -ne "Both plugin variants require an installed UE 5.8 engine with Build.version.") { throw }
		}
	}
	foreach ($version in @(@{ MajorVersion = 5; MinorVersion = 7 }, @{ MajorVersion = 5; MinorVersion = 9 }, @{ MajorVersion = 6; MinorVersion = 0 })) {
		$version | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $testRoot "Build/Build.version") -Encoding UTF8
		foreach ($variant in @("Standard", "MCP")) {
			try {
				Assert-PlaneToPBREngine -EngineDir $testRoot -Variant $variant
				throw "$variant accepted an engine outside UE 5.8."
			} catch {
				if ($_.Exception.Message -notlike "Both plugin variants require UE 5.8;*") { throw }
			}
		}
	}
	@{ MajorVersion = 5; MinorVersion = 8; PatchVersion = 1 } |
		ConvertTo-Json | Set-Content -LiteralPath (Join-Path $testRoot "Build/Build.version") -Encoding UTF8
	Assert-PlaneToPBREngine -EngineDir $testRoot -Variant Standard
	try {
		Assert-PlaneToPBREngine -EngineDir $testRoot -Variant MCP
		throw "MCP accepted an engine without its required plugins."
	} catch {
		if ($_.Exception.Message -ne "MCP engine integration unavailable: expected one ModelContextProtocol.uplugin.") { throw }
	}
	Write-Host "PlaneToPBR shared UE 5.8 engine safeguards passed."
} finally {
	Remove-Item -LiteralPath $testRoot -Recurse -Force
}