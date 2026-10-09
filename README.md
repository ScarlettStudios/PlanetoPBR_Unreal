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

## Installation

PlaneToPBR is packaged and tested for Win64 in two alternative distributions:

- `PlaneToPBR.zip` is the standard plugin with the existing editor UI, built for Unreal Engine 5.8 without MCP dependencies.
- `PlaneToPBRMCP.zip` is a standalone MCP superset with the same editor UI plus MCP tools, built with the same Unreal Engine 5.8 installation. It requires the engine's `ModelContextProtocol` and `ToolsetRegistry` plugins.

Both archives contain the same plugin identity and folder name, `PlaneToPBR`. Install only one variant at a time. To switch variants, close Unreal Editor, remove or replace the existing `Plugins/PlaneToPBR` folder, and then install the other archive; do not install both or retain another engine-level copy.

Watch the How-to Video Here: https://youtu.be/Cuf8gHKDcQE?si=qsAuYUTM80tlW7VT

This branch targets Unreal Engine 5.8 on Win64 for both distributions. The previously released UE 5.7 package remains available in its existing release; new local builds use the artifact paths below and overwrite any previous packages at those paths.

### Install From GitHub Release

1. Open the repository's GitHub Releases page.
2. Download `PlaneToPBR.zip` for the standard plugin or `PlaneToPBRMCP.zip` for the MCP superset from the latest PlaneToPBR release.
3. Extract the selected ZIP.
4. Copy the extracted `PlaneToPBR` folder into your Unreal project's `Plugins` folder. Create `Plugins` if the project does not already have one.
5. Restart Unreal Editor.
6. Open `Edit > Plugins`, search for `PlaneToPBR`, and confirm the plugin is enabled.
7. Restart Unreal Editor again if Unreal asks you to.

The installed plugin should be located at:

```text
YourProject/Plugins/PlaneToPBR/PlaneToPBR.uplugin
```

### Install From GitHub Actions Artifact

Before a GitHub Release is published, testers can install the package produced by the packaging workflow:

1. Open the repository's `Actions` tab.
2. Open the latest successful `Package PlaneToPBR Plugin` workflow run for the target branch or tag.
3. Download `PlaneToPBR-plugin` for the standard variant or `PlaneToPBRMCP-plugin` for the MCP variant.
4. Extract the artifact ZIP to get `PlaneToPBR.zip` or `PlaneToPBRMCP.zip`.
5. Extract the selected plugin ZIP.
6. Copy the extracted `PlaneToPBR` folder into your Unreal project's `Plugins` folder.
7. Restart Unreal Editor and enable the plugin if prompted.

### Install From Local Package

To install from a locally packaged ZIP:

1. Extract `PlaneToPBR.zip` or `PlaneToPBRMCP.zip`, depending on the desired variant.
2. Copy the extracted `PlaneToPBR` folder into your Unreal project's `Plugins` folder.
3. Restart Unreal Editor.
4. Open `Edit > Plugins`, search for `PlaneToPBR`, and confirm the plugin is enabled.
5. Restart Unreal Editor again if Unreal asks you to.

## Usage

Open the editor tool from:

```text
Tools > PlaneToPBR
```

Workflow:

1. Pick a source image.
2. Enter an optional prompt.
3. Click `Generate PBR Plane`.
4. Wait while PlaneToPBR uploads the image, generates maps through the managed Hugging Face Space, downloads the generated textures, imports assets, creates a material, and places a displaced plane actor in the current level.
5. Inspect the generated assets and actor in the editor.

PlaneToPBR imports generated Unreal assets under:

```text
Content/PlaneToPBR/Generated/
```

Each generation run creates a timestamped folder containing imported textures, the generated material, and the displaced static mesh.

Downloaded intermediate PNG files are written under:

```text
Saved/PlaneToPBR/Generated/
```

Intermediate OBJ mesh files are written under:

```text
Saved/PlaneToPBR/Meshes/
```

The `Saved/` files are temporary/generated support files. The imported assets under `Content/PlaneToPBR/Generated/` are the assets to inspect and use in the project.

## Implementation Workflow

The editor workflow performs these steps:

1. Upload the selected source image.
2. Join and poll the managed Hugging Face generation queue.
3. Download generated depth, normal, roughness, and mask PNG files.
4. Import the generated maps as persistent Unreal assets.
5. Create a generated material.
6. Create and place a displaced plane in the level.

`Content/PlaneToPBR/Generated/` is generated-only and ignored by source control.

## Hugging Face Space

PlaneToPBR uses a managed Hugging Face Space. The endpoint is fixed by product design and is not configurable by users.

The plugin calls:

```text
https://ascarlettvfx-testpbr2026.hf.space
```

When generation starts, the plugin uploads the selected source image and the optional prompt text to that managed Space. Do not use private or sensitive source images unless you are comfortable sending them to the PlaneToPBR generation service.

The expected Gradio API route is:

```text
api_name="predict"
```

The Space must return four PNG file outputs in this order:

```text
depth, normal, roughness, mask
```

The source image is used as the diffuse map.

Common service failure cases:

- If the Space is sleeping or unavailable, generation may fail with an HTTP error or a connection message. Wait briefly and try again.
- If the network is unavailable, the plugin reports that it failed to connect to Hugging Face.
- If the selected input image cannot be processed, the Space may return a generation failure. Try a different image or prompt.
- If the managed Space API changes unexpectedly, the plugin may report invalid JSON, missing fields, or missing output URLs.

## Packaging

PlaneToPBR is packaged with Unreal Automation Tool `BuildPlugin`.

The packaging script lives at:

```text
Build/PackagePlaneToPBRPlugin.ps1
```

The source descriptor remains standard-only. Each invocation stages one isolated variant before building it:

```text
Standard: PlaneToPBR and PlaneToPBREditor
MCP:      PlaneToPBR, PlaneToPBREditor, and PlaneToPBRMCP
```

The deliverables are written to:

```text
Artifacts/PlaneToPBR
Artifacts/PlaneToPBR.zip
Artifacts/MCP/PlaneToPBR
Artifacts/PlaneToPBRMCP.zip
```

`Artifacts/` is generated-only and ignored by source control.

### Local Windows Packaging

Set one UE 5.8 installation for both variants, then test and package each in its isolated output tree:

```powershell
$env:UE_ENGINE_DIR = "C:\Program Files\Epic Games\UE_5.8"
$env:PLUGIN_TARGET_PLATFORMS = "Win64"
.\Build\TestPlaneToPBRPackaging.ps1
.\Build\RunPlaneToPBRAutomationTests.ps1 -Variant Standard
.\Build\PackagePlaneToPBRPlugin.ps1 -Variant Standard
.\Build\SmokePlaneToPBRPackage.ps1 -Variant Standard
.\Build\RunPlaneToPBRAutomationTests.ps1 -Variant MCP
.\Build\PackagePlaneToPBRPlugin.ps1 -Variant MCP
.\Build\SmokePlaneToPBRPackage.ps1 -Variant MCP
```

GitHub Actions uses the same `ue-engine-dir` input (or `UE_ENGINE_DIR` repository variable) for both matrix entries, defaulting to `C:\Program Files\Epic Games\UE_5.8`. Separate standard/MCP engine settings are no longer used. Both variants reject engines outside UE 5.8; only MCP checks for the MCP engine plugins.

### Docker Packaging

The Docker image in this repo does not include or download Unreal Engine, Visual Studio Build Tools, or Windows SDK components. It expects those dependencies to be available in the container environment through mounted paths or a prebuilt image.

For CI, prefer the self-hosted Windows runner workflow below. Unreal's Windows build prerequisites are large and version-sensitive, so the Dockerfile is kept as a lightweight wrapper instead of installing the toolchain during every image build.

Build the Docker image:

```powershell
docker build -t planetopbr-plugin-packager .
```

Build and run the automation-test stage:

```powershell
docker build --target test -t planetopbr-plugin-tests .

docker run --rm `
  -e UE_ENGINE_DIR=C:\UnrealEngine `
  -v "C:\Program Files\Epic Games\UE_5.8:C:\UnrealEngine" `
  planetopbr-plugin-tests
```

Mount a Windows Unreal Engine installation into the container and set `UE_ENGINE_DIR` to that mounted path:

```powershell
docker run --rm `
  -e UE_ENGINE_DIR=C:\UnrealEngine `
  -e PLUGIN_TARGET_PLATFORMS=Win64 `
  -v "C:\Program Files\Epic Games\UE_5.8:C:\UnrealEngine" `
  -v "${PWD}/Artifacts:C:\workspace\Artifacts" `
  planetopbr-plugin-packager
```

The default Docker target is the `package` stage, which runs `Build/RunPlaneToPBRAutomationTests.ps1` before `Build/PackagePlaneToPBRPlugin.ps1`.
Docker must be running Windows containers for this image.
