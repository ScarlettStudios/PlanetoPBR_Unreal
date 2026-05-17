# PlaneToPBR Plugin Packaging

PlaneToPBR is packaged with Unreal Automation Tool `BuildPlugin`.

The Docker image in this repo does not include or download Unreal Engine. It expects an Unreal Engine installation to be available in the container or runner through `UE_ENGINE_DIR`.

## Build Docker Image

```powershell
docker build -t planetopbr-plugin-packager .
```

## Run Packaging In Docker

Mount an Unreal Engine installation into the container and set `UE_ENGINE_DIR` to that mounted path.

```powershell
docker run --rm `
  -e UE_ENGINE_DIR=/opt/unreal-engine `
  -e PLUGIN_TARGET_PLATFORMS=Linux `
  -v /path/to/UnrealEngine:/opt/unreal-engine `
  -v ${PWD}/Artifacts:/workspace/Artifacts `
  planetopbr-plugin-packager
```

The packaged plugin is written to:

```text
Artifacts/PlaneToPBR
Artifacts/PlaneToPBR.zip
```

## Local Windows Packaging

```powershell
$env:UE_ENGINE_DIR = "C:\Program Files\Epic Games\UE_5.7"
$env:PLUGIN_TARGET_PLATFORMS = "Win64"
.\Scripts\PackagePlaneToPBRPlugin.ps1
```

## GitHub Actions

The workflow at `.github/workflows/package-plugin.yml` builds the Docker image and uses the same packaging script. It expects:

- a Linux self-hosted runner,
- Docker available to the runner,
- Unreal Engine available inside the container at the configured `UE_ENGINE_DIR`.

Use `workflow_dispatch` to select the target platform and engine directory.
