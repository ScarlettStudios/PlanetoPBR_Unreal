param(
	[ValidateSet("Standard", "MCP")][string]$Variant = "Standard",
	[string]$ZipPath
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "PlaneToPBRVariants.ps1")
if (-not $ZipPath) { $ZipPath = if ($Variant -eq "Standard") { "Artifacts/PlaneToPBR.zip" } else { "Artifacts/PlaneToPBRMCP.zip" } }
$zip = [IO.Path]::GetFullPath((Join-Path (Get-Location) $ZipPath))
Assert-PlaneToPBRArchive -ZipPath $zip -Variant $Variant
$engine = $env:UE_ENGINE_DIR
if (Test-Path -LiteralPath (Join-Path $engine "Engine")) { $engine = Join-Path $engine "Engine" }
Assert-PlaneToPBREngine -EngineDir $engine -Variant $Variant
$hostRoot = Join-Path (Split-Path -Parent $PSScriptRoot) "Artifacts/Variants/$Variant/SmokeHost"
if (Test-Path -LiteralPath $hostRoot) { Remove-Item -LiteralPath $hostRoot -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $hostRoot "Plugins") -Force | Out-Null
[IO.Compression.ZipFile]::ExtractToDirectory($zip, (Join-Path $hostRoot "Plugins"))
Assert-PlaneToPBRVariant -Root (Join-Path $hostRoot "Plugins/PlaneToPBR") -Variant $Variant -RequireBinaries
$project = Join-Path $hostRoot "SmokeHost.uproject"
@{
	FileVersion = 3
	Plugins = @(@{ Name = "PlaneToPBR"; Enabled = $true }, @{ Name = "PythonScriptPlugin"; Enabled = $true })
} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $project -Encoding UTF8
$env:PLANETOPBR_SMOKE_VARIANT = $Variant
$python = Join-Path $PSScriptRoot "SmokePlaneToPBRPackage.py"
& (Join-Path $engine "Binaries/Win64/UnrealEditor-Cmd.exe") $project "-ExecutePythonScript=$python" -unattended -nop4 -nosplash -nullrhi -stdout -FullStdOutLogOutput
if ($LASTEXITCODE -ne 0) { throw "Packaged plugin smoke editor exited with $LASTEXITCODE." }
$marker = Join-Path $hostRoot "Saved/PlaneToPBRSmokePassed.txt"
if (-not (Test-Path -LiteralPath $marker)) { throw "Packaged plugin smoke assertions did not pass." }
Write-Host "$Variant packaged ZIP independently loaded and passed smoke assertions."