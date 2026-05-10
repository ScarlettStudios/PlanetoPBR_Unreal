# PlaneToPBR Unreal

Unreal recreation of the Blender PlaneToPBR workflow.

The project sends a source image and optional prompt to the same Hugging Face Space used by the Blender extension, downloads generated PBR maps, and creates a textured plane in Unreal.

## Current Status

This is a working Unreal proof of concept.

Implemented:

- Hugging Face Gradio queue client
- Same HF Space endpoint as the Blender project
- Image upload
- Queue join and polling
- PNG map download
- Runtime texture decoding
- Generated plane actor
- Auto test actor
- Base PlaneToPBR material

Generated maps:

- Diffuse
- Depth
- Normal
- Roughness
- Prompt-based mask

## Hugging Face Space

The project calls:

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

The source image is saved locally as the diffuse map.

## Testing In Unreal

Use the built-in test actor:

```text
PlanetoPBRGeneratorActor
```

Steps:

1. Open the project in Unreal.
2. Open or create a level.
3. Place `PlanetoPBRGeneratorActor` in the level.
4. Set `ImagePath`, or use the default:

```text
F:\test_image.png
```

5. Set `Prompt`, for example:

```text
windows
```

6. Press Play.

Expected output appears in:

```text
Saved/PlaneToPBR_textures
```

Check the Output Log for:

```text
LogPlanetoPBR
LogPlanetoPBRGenerator
```

## Material

The base material is:

```text
/Game/PlaneToPBR/M_PlaneToPBR
```

Expected texture parameters:

- `BaseColorTexture`
- `NormalTexture`
- `RoughnessTexture`
- `DepthTexture`
- `MaskTexture`

## Verification Script

The HF Space can be checked from Unreal's embedded Python runtime:

```text
Tools/VerifyHFSpaceFromUnreal.py
```

It verifies:

- `api_name="predict"` exists
- `fn_index` resolves
- upload works
- queue processing completes
- all four generated outputs are PNG files

## Important Folders

Track in source control:

```text
Config/
Content/PlaneToPBR/
Source/
Tools/
planetoPBR_unreal.uproject
.vsconfig
```

Do not track generated folders:

```text
Binaries/
DerivedDataCache/
Intermediate/
Saved/
.vs/
.idea/
```

These are covered by `.gitignore`.

## Blender Reference Project

Original Blender extension project:

```text
C:\Users\joshu\RiderProjects\PlaneToPBR
```

The Unreal implementation mirrors the Blender free Hugging Face flow:

1. Select image
2. Enter prompt
3. Call Hugging Face
4. Download PBR maps
5. Create a plane
6. Apply generated material

## Next Plugin Direction

For a production Unreal extension, the next step is to convert this project code into an Editor Plugin.

Recommended final workflow:

1. Open **Window > PlaneToPBR**
2. Pick an image
3. Enter prompt
4. Click Generate
5. Import generated PNG maps as persistent `.uasset` textures
6. Create a material instance
7. Spawn a correctly scaled plane in the level

The main remaining gap is persistent asset import. The current proof of concept saves generated PNG files under `Saved/` and creates runtime textures.
