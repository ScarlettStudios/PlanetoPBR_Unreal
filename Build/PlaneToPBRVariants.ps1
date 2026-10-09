function Get-PlaneToPBRModules {
	param([string]$Variant)
	$modules = @("PlaneToPBR", "PlaneToPBREditor")
	if ($Variant -eq "MCP") { $modules += "PlaneToPBRMCP" }
	return $modules
}

function Assert-PlaneToPBREngine {
	param([string]$EngineDir, [string]$Variant)
	$versionFile = Join-Path $EngineDir "Build/Build.version"
	if (-not (Test-Path -LiteralPath $versionFile)) { throw "Both plugin variants require an installed UE 5.8 engine with Build.version." }
	$version = Get-Content -Raw -LiteralPath $versionFile | ConvertFrom-Json
	if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8) {
		throw "Both plugin variants require UE 5.8; this installation is $($version.MajorVersion).$($version.MinorVersion)."
	}
	if ($Variant -ne "MCP") { return }
	foreach ($name in @("ModelContextProtocol", "ToolsetRegistry")) {
		$plugins = @(Get-ChildItem -LiteralPath (Join-Path $EngineDir "Plugins") -Recurse -File -Filter "$name.uplugin")
		if ($plugins.Count -ne 1) { throw "MCP engine integration unavailable: expected one $name.uplugin." }
		$descriptor = Get-Content -Raw -LiteralPath $plugins[0].FullName | ConvertFrom-Json
		$requiredModules = if ($name -eq "ModelContextProtocol") { @("ModelContextProtocol", "ModelContextProtocolEngine", "ModelContextProtocolEditor") } else { @("ToolsetRegistry") }
		foreach ($module in $requiredModules) {
			if ($descriptor.Modules.Name -notcontains $module -or -not (Test-Path -LiteralPath (Join-Path $plugins[0].DirectoryName "Source/$module/$module.Build.cs"))) {
				throw "MCP engine integration is missing supported module: $module"
			}
		}
	}
}

function Test-PlaneToPBRPathOverlap {
	param([string]$First, [string]$Second)
	$firstPath = [IO.Path]::GetFullPath($First).TrimEnd('\', '/')
	$secondPath = [IO.Path]::GetFullPath($Second).TrimEnd('\', '/')
	return $firstPath.Equals($secondPath, [StringComparison]::OrdinalIgnoreCase) -or
		$firstPath.StartsWith($secondPath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
		$secondPath.StartsWith($firstPath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
}

function Assert-PlaneToPBROutputPaths {
	param([string]$Variant, [string]$PluginFile, [string[]]$Paths, [string]$EngineDir)
	$repo = Split-Path -Parent $PSScriptRoot
	$artifacts = Join-Path $repo "Artifacts"
	$other = if ($Variant -eq "Standard") { "MCP" } else { "Standard" }
	$protected = @((Split-Path -Parent $PluginFile), $EngineDir, (Join-Path $artifacts "Variants/$other"),
		(Join-Path $artifacts "Variants/$Variant/Paths.json"))
	$protected += if ($other -eq "Standard") { @((Join-Path $artifacts "PlaneToPBR"), (Join-Path $artifacts "PlaneToPBR.zip")) } else { @((Join-Path $artifacts "MCP/PlaneToPBR"), (Join-Path $artifacts "PlaneToPBRMCP.zip")) }
	$otherRecord = Join-Path $artifacts "Variants/$other/Paths.json"
	if (Test-Path -LiteralPath $otherRecord) {
		$recordPaths = Get-Content -Raw -LiteralPath $otherRecord | ConvertFrom-Json
		$protected += $recordPaths
	}
	for ($i = 0; $i -lt $Paths.Count; $i++) {
		$path = [IO.Path]::GetFullPath($Paths[$i])
		if (-not $path.StartsWith($artifacts + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
			throw "Output paths must be below $artifacts to prevent destructive cleanup: $path"
		}
		foreach ($reserved in $protected) {
			if (Test-PlaneToPBRPathOverlap $path $reserved) { throw "Output path collides with protected source, engine, or variant output: $path" }
		}
		for ($j = $i + 1; $j -lt $Paths.Count; $j++) {
			if (Test-PlaneToPBRPathOverlap $path $Paths[$j]) { throw "Selected variant output paths overlap: $path and $($Paths[$j])" }
		}
	}
	$record = Join-Path $artifacts "Variants/$Variant/Paths.json"
	New-Item -ItemType Directory -Path (Split-Path -Parent $record) -Force | Out-Null
	ConvertTo-Json -InputObject @($Paths) | Set-Content -LiteralPath $record -Encoding UTF8
}

function New-PlaneToPBRStage {
	param([string]$PluginFile, [string]$Destination, [string]$Variant)
	$source = Split-Path -Parent $PluginFile
	$descriptor = Get-Content -Raw -LiteralPath $PluginFile | ConvertFrom-Json
	if ((($descriptor.Modules.Name | Sort-Object) -join ',') -ne 'PlaneToPBR,PlaneToPBREditor' -or @($descriptor.Plugins | Where-Object { $_ }).Count -gt 0) {
		throw "Source descriptor must contain only the standard modules and no MCP dependencies."
	}
	if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Recurse -Force }
	New-Item -ItemType Directory -Path (Join-Path $Destination "Source") -Force | Out-Null
	foreach ($module in Get-PlaneToPBRModules $Variant) {
		Copy-Item -LiteralPath (Join-Path $source "Source/$module") -Destination (Join-Path $Destination "Source") -Recurse
	}
	foreach ($resource in @("Config", "Resources", "LICENSE", "LICENSE.md", "README.md")) {
		$path = Join-Path $source $resource
		if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination $Destination -Recurse }
	}
	if ($Variant -eq "MCP") {
		$descriptor.Modules += [pscustomobject]@{ Name = "PlaneToPBRMCP"; Type = "Editor"; LoadingPhase = "PostEngineInit" }
		$descriptor | Add-Member -NotePropertyName Plugins -NotePropertyValue @(
			[pscustomobject]@{ Name = "ToolsetRegistry"; Enabled = $true },
			[pscustomobject]@{ Name = "ModelContextProtocol"; Enabled = $true })
	}
	$descriptor | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $Destination "PlaneToPBR.uplugin") -Encoding UTF8
}

function Assert-PlaneToPBRVariant {
	param([string]$Root, [string]$Variant, [switch]$RequireBinaries)
	$modules = @(Get-PlaneToPBRModules $Variant)
	$descriptor = Get-Content -Raw -LiteralPath (Join-Path $Root "PlaneToPBR.uplugin") | ConvertFrom-Json
	if ((($descriptor.Modules.Name | Sort-Object) -join ',') -ne (($modules | Sort-Object) -join ',')) { throw "Descriptor module contamination in $Root" }
	$plugins = @($descriptor.Plugins | Where-Object { $_ }).Name
	$expectedPlugins = if ($Variant -eq "MCP") { @("ModelContextProtocol", "ToolsetRegistry") } else { @() }
	if ((($plugins | Sort-Object) -join ',') -ne (($expectedPlugins | Sort-Object) -join ',')) { throw "Descriptor dependency contamination in $Root" }
	$sourceModules = @(Get-ChildItem -LiteralPath (Join-Path $Root "Source") -Directory).Name
	if ((($sourceModules | Sort-Object) -join ',') -ne (($modules | Sort-Object) -join ',')) { throw "Source module contamination in $Root" }
	$binaryModules = @()
	foreach ($file in Get-ChildItem -LiteralPath $Root -Recurse -File) {
		if ($Variant -eq "Standard" -and $file.FullName.Substring($Root.Length) -match 'MCP|ModelContextProtocol|ToolsetRegistry') { throw "MCP file in standard output: $($file.FullName)" }
		if ($file.Extension -eq '.modules') {
			$manifest = Get-Content -Raw -LiteralPath $file.FullName | ConvertFrom-Json
			foreach ($property in $manifest.Modules.PSObject.Properties) {
				if ($modules -notcontains $property.Name) { throw "Unexpected binary module: $($property.Name)" }
				$binaryModules += $property.Name
			}
		}
		if ($Variant -eq "Standard" -and $file.Extension -in @('.cs', '.h', '.cpp', '.json', '.uplugin', '.modules', '.precompiled', '.target', '.xml')) {
			if ((Get-Content -Raw -LiteralPath $file.FullName) -match 'PlaneToPBRMCP|ModelContextProtocol|ToolsetRegistry') { throw "MCP reference in standard output: $($file.FullName)" }
		}
	}
	if ($RequireBinaries) {
		foreach ($module in $modules) {
			if ($binaryModules -notcontains $module) { throw "Missing compiled module: $module" }
		}
	}
}

function Assert-PlaneToPBRArchive {
	param([string]$ZipPath, [string]$Variant, [switch]$IncludeTests, [switch]$IncludeDebugSymbols)
	Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
	$archive = [IO.Compression.ZipFile]::OpenRead($ZipPath)
	try {
		$descriptorEntry = $archive.GetEntry("PlaneToPBR/PlaneToPBR.uplugin")
		if (-not $descriptorEntry) { throw "ZIP does not contain independently installable PlaneToPBR root." }
		$reader = [IO.StreamReader]::new($descriptorEntry.Open())
		try { $descriptor = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
		if ((($descriptor.Modules.Name | Sort-Object) -join ',') -ne (((Get-PlaneToPBRModules $Variant) | Sort-Object) -join ',')) { throw "ZIP has incorrect module set." }
		foreach ($entry in $archive.Entries) {
			if (-not $entry.FullName.StartsWith('PlaneToPBR/') -or $entry.FullName -match '(^|/)\.\.(/|$)') { throw "Invalid ZIP root or path: $($entry.FullName)" }
			if ($Variant -eq "Standard" -and $entry.FullName -match 'MCP|ModelContextProtocol|ToolsetRegistry') { throw "MCP contamination in standard ZIP." }
			if (-not $IncludeTests -and $entry.FullName -match '/Tests/|Tests\.') { throw "Automation test files in release ZIP." }
			if (-not $IncludeDebugSymbols -and $entry.FullName -like '*.pdb') { throw "Debug symbols in release ZIP." }
		}
	} finally { $archive.Dispose() }
}