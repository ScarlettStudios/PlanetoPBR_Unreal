import os
import re

import unreal


TEXTURE_SET_DIR = os.path.join(unreal.Paths.project_saved_dir(), "PlaneToPBR_textures")
ASSET_ROOT = "/Game/PlaneToPBR/Generated"
MATERIAL_PREFIX = "M"
TEXTURE_PREFIX = "T"

MAP_DEFINITIONS = {
    "diffuse": {
        "asset_suffix": "BaseColor",
        "parameter": "BaseColorTexture",
        "srgb": True,
        "sampler": "SAMPLERTYPE_COLOR",
        "material_property": unreal.MaterialProperty.MP_BASE_COLOR,
        "output": "RGB",
    },
    "normal": {
        "asset_suffix": "Normal",
        "parameter": "NormalTexture",
        "srgb": False,
        "sampler": "SAMPLERTYPE_NORMAL",
        "compression": "TC_NORMALMAP",
        "material_property": unreal.MaterialProperty.MP_NORMAL,
        "output": "RGB",
    },
    "roughness": {
        "asset_suffix": "Roughness",
        "parameter": "RoughnessTexture",
        "srgb": False,
        "sampler": "SAMPLERTYPE_MASKS",
        "compression": "TC_MASKS",
        "material_property": unreal.MaterialProperty.MP_ROUGHNESS,
        "output": "R",
    },
    "depth": {
        "asset_suffix": "Depth",
        "parameter": "DepthTexture",
        "srgb": False,
        "sampler": "SAMPLERTYPE_MASKS",
        "compression": "TC_MASKS",
    },
    "mask": {
        "asset_suffix": "Mask",
        "parameter": "MaskTexture",
        "srgb": False,
        "sampler": "SAMPLERTYPE_MASKS",
        "compression": "TC_MASKS",
    },
}

TEXTURE_FILE_PATTERN = re.compile(r"^(diffuse|depth|normal|roughness|mask)_(.+)\.(png|jpg|jpeg|tga|exr)$", re.IGNORECASE)


def log(message):
    print(f"PLANETOPBR_IMPORT: {message}")


def sanitize_asset_name(value):
    sanitized = re.sub(r"[^A-Za-z0-9_]+", "_", value).strip("_")
    return sanitized or "Generated"


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def find_texture_sets(texture_dir=TEXTURE_SET_DIR):
    if not os.path.isdir(texture_dir):
        raise RuntimeError(f"Texture directory does not exist: {texture_dir}")

    grouped = {}
    for filename in os.listdir(texture_dir):
        match = TEXTURE_FILE_PATTERN.match(filename)
        if not match:
            continue

        map_type = match.group(1).lower()
        run_name = match.group(2)
        grouped.setdefault(run_name, {})[map_type] = os.path.join(texture_dir, filename)

    complete_sets = {
        run_name: files
        for run_name, files in grouped.items()
        if all(map_type in files for map_type in MAP_DEFINITIONS)
    }
    if not complete_sets:
        raise RuntimeError(f"No complete PlaneToPBR texture set found in: {texture_dir}")

    return complete_sets


def find_latest_texture_set(texture_dir=TEXTURE_SET_DIR):
    complete_sets = find_texture_sets(texture_dir)

    def newest_mtime(item):
        _, files = item
        return max(os.path.getmtime(path) for path in files.values())

    run_name, files = max(complete_sets.items(), key=newest_mtime)
    return sanitize_asset_name(run_name), files


def enum_value(enum_type, name, fallback_name=None):
    if hasattr(enum_type, name):
        return getattr(enum_type, name)
    if fallback_name and hasattr(enum_type, fallback_name):
        return getattr(enum_type, fallback_name)
    return None


def safe_set_editor_property(obj, property_name, value):
    try:
        obj.set_editor_property(property_name, value)
        return True
    except Exception as exc:
        log(f"skipped_property {obj.get_name()}.{property_name}: {exc}")
        return False


def safe_call(obj, method_name):
    method = getattr(obj, method_name, None)
    if not method:
        log(f"skipped_method {obj.get_name()}.{method_name}")
        return False

    try:
        method()
        return True
    except Exception as exc:
        log(f"skipped_method {obj.get_name()}.{method_name}: {exc}")
        return False


def import_texture(file_path, destination_path, asset_name, definition):
    task = unreal.AssetImportTask()
    task.filename = file_path
    task.destination_path = destination_path
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = True

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported_paths = list(task.imported_object_paths)
    asset_path = imported_paths[0] if imported_paths else f"{destination_path}/{asset_name}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not texture:
        raise RuntimeError(f"Failed to import texture: {file_path}")

    safe_set_editor_property(texture, "srgb", definition["srgb"])

    compression_name = definition.get("compression")
    if compression_name:
        compression = enum_value(unreal.TextureCompressionSettings, compression_name)
        if compression is not None:
            safe_set_editor_property(texture, "compression_settings", compression)

    safe_call(texture, "modify")
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def create_texture_parameter(material, texture, definition, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        x,
        y,
    )
    node.set_editor_property("parameter_name", definition["parameter"])
    node.set_editor_property("texture", texture)

    sampler = enum_value(
        unreal.MaterialSamplerType,
        definition["sampler"],
        "SAMPLERTYPE_COLOR",
    )
    if sampler is not None:
        node.set_editor_property("sampler_type", sampler)

    return node


def create_material(destination_path, run_name, imported_textures):
    material_name = f"{MATERIAL_PREFIX}_{run_name}_PlaneToPBR"
    material_path = f"{destination_path}/{material_name}"

    existing = None
    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        existing = unreal.EditorAssetLibrary.load_asset(material_path)
    if existing:
        material = existing
    else:
        factory = unreal.MaterialFactoryNew()
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name,
            destination_path,
            unreal.Material,
            factory,
        )

    if not material:
        raise RuntimeError(f"Failed to create material: {material_path}")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    positions = {
        "diffuse": (-700, -300),
        "normal": (-700, -80),
        "roughness": (-700, 140),
        "depth": (-700, 360),
        "mask": (-700, 580),
    }

    for map_type, definition in MAP_DEFINITIONS.items():
        node = create_texture_parameter(
            material,
            imported_textures[map_type],
            definition,
            positions[map_type][0],
            positions[map_type][1],
        )
        material_property = definition.get("material_property")
        output = definition.get("output")
        if material_property is not None and output:
            unreal.MaterialEditingLibrary.connect_material_property(
                node,
                output,
                material_property,
            )

    material.set_editor_property("use_material_attributes", False)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def import_latest_generated_material(texture_dir=TEXTURE_SET_DIR, asset_root=ASSET_ROOT):
    run_name, files = find_latest_texture_set(texture_dir)
    destination_path = f"{asset_root}/{run_name}"
    ensure_directory(asset_root)
    ensure_directory(destination_path)

    log(f"run_name={run_name}")
    log(f"texture_dir={texture_dir}")
    log(f"destination_path={destination_path}")

    imported_textures = {}
    for map_type, definition in MAP_DEFINITIONS.items():
        asset_name = f"{TEXTURE_PREFIX}_{run_name}_{definition['asset_suffix']}"
        imported_textures[map_type] = import_texture(
            files[map_type],
            destination_path,
            asset_name,
            definition,
        )
        log(f"imported_{map_type}={imported_textures[map_type].get_path_name()}")

    material = create_material(destination_path, run_name, imported_textures)
    log(f"material={material.get_path_name()}")
    return material


if __name__ == "__main__":
    import_latest_generated_material()
