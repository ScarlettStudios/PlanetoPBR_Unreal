# PlaneToPBR Unreal

PlaneToPBR is an Unreal Engine plugin that generates PBR material assets and displaced plane meshes from a source image.

The product code lives under:

```text
Plugins/PlaneToPBR
```

The Unreal project in this repository is a host project for plugin development, packaging, and testing.

## Current Status

PlaneToPBR is a plugin-first Unreal implementation of the Blender PlaneToPBR workflow.

Implemented:

- Hugging Face Gradio queue client
- Source image upload
- Queue join and polling
- PNG map download
- Persistent texture asset import
- Generated material creation
- Displaced plane mesh import and actor placement
- Editor window entry point

Generated maps:

- Diffuse
- Depth
- Normal
- Roughness
- Prompt-based mask

## Plugin Workflow

Open the editor tool from:

```text
Tools > PlaneToPBR
```

Workflow:

1. Pick a source image.
2. Enter an optional prompt.
3. Generate PBR maps through the configured Hugging Face Space.
4. Import the generated maps as persistent Unreal assets.
5. Create a generated material.
6. Create and place a displaced plane in the level.

Generated plugin outputs are written under:

```text
Content/PlaneToPBR/Generated/
```

That folder is generated-only and ignored by source control.

## Hugging Face Space

The plugin calls:

```text
https://ascarlettvfx-testpbr2026.hf.space
```

The expected Gradio API route is:

```text
api_name="predict"
```

The Space must return four PNG file outputs in this order:

```text
depth, normal, roughness, mask
```

The source image is used as the diffuse map.

## Packaging

PlaneToPBR is packaged with Unreal Automation Tool `BuildPlugin`.

The packaging script lives at:

```text
Build/PackagePlaneToPBRPlugin.ps1
```

It packages only:

```text
Plugins/PlaneToPBR/PlaneToPBR.uplugin
```

The packaged plugin is written to:

```text
Artifacts/PlaneToPBR
Artifacts/PlaneToPBR.zip
```

`Artifacts/` is generated-only and ignored by source control.

### Local Windows Packaging

```powershell
$env:UE_ENGINE_DIR = "C:\Program Files\Epic Games\UE_5.7"
$env:PLUGIN_TARGET_PLATFORMS = "Win64"
.\Build\PackagePlaneToPBRPlugin.ps1
```

### Docker Packaging

The Docker image in this repo does not include or download Unreal Engine. It expects an Unreal Engine installation to be available in the container or runner through `UE_ENGINE_DIR`.

Build the Docker image:

```powershell
docker build -t planetopbr-plugin-packager .
```

Mount an Unreal Engine installation into the container and set `UE_ENGINE_DIR` to that mounted path:

```powershell
docker run --rm `
  -e UE_ENGINE_DIR=/opt/unreal-engine `
  -e PLUGIN_TARGET_PLATFORMS=Linux `
  -v /path/to/UnrealEngine:/opt/unreal-engine `
  -v ${PWD}/Artifacts:/workspace/Artifacts `
  planetopbr-plugin-packager
```

### GitHub Actions

The workflow at `.github/workflows/package-plugin.yml` uses the same packaging script. It expects:

- a Windows self-hosted runner,
- Unreal Engine installed on the runner,
- Visual Studio Build Tools available to Unreal Build Tool,
- `UE_ENGINE_DIR` configured as a workflow input or repository variable.

Use `workflow_dispatch` to select the target platform and engine directory.

## Repository Layout

Track in source control:

```text
.github/
Build/
Config/
Content/Maps/
Content/__ExternalActors__/
Content/__ExternalObjects__/
Plugins/PlaneToPBR/
Source/planetoPBR_unreal/
Dockerfile
README.md
planetoPBR_unreal.uproject
```

Do not track generated folders:

```text
Artifacts/
Binaries/
DerivedDataCache/
Intermediate/
Saved/
Content/PlaneToPBR/Generated/
Plugins/**/Binaries/
Plugins/**/Intermediate/
.vs/
.idea/
```

These are covered by `.gitignore`.

## Host Project

The root Unreal project exists only to host and test the plugin. Keep product behavior in `Plugins/PlaneToPBR`; keep `Source/planetoPBR_unreal` limited to minimal game module boilerplate.
