import unreal


ASSET_DIR = "/Game/PlaneToPBR"
MATERIAL_NAME = "M_PlaneToPBR"
MATERIAL_PATH = f"{ASSET_DIR}/{MATERIAL_NAME}"


def ensure_directory(path):
    editor_asset_lib = unreal.EditorAssetLibrary
    if not editor_asset_lib.does_directory_exist(path):
        editor_asset_lib.make_directory(path)


def create_or_load_material():
    existing = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if existing:
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    return asset_tools.create_asset(MATERIAL_NAME, ASSET_DIR, unreal.Material, factory)


def add_texture_parameter(material, name, x, y, sampler_type):
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        x,
        y,
    )
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("sampler_type", sampler_type)
    return node


def main():
    ensure_directory(ASSET_DIR)
    material = create_or_load_material()
    if not material:
        raise RuntimeError("Failed to create M_PlaneToPBR")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    base_color = add_texture_parameter(
        material,
        "BaseColorTexture",
        -600,
        -250,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    )
    normal = add_texture_parameter(
        material,
        "NormalTexture",
        -600,
        0,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    )
    roughness = add_texture_parameter(
        material,
        "RoughnessTexture",
        -600,
        250,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    )
    depth = add_texture_parameter(
        material,
        "DepthTexture",
        -250,
        450,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    )
    mask = add_texture_parameter(
        material,
        "MaskTexture",
        -250,
        650,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    )

    unreal.MaterialEditingLibrary.connect_material_property(
        base_color,
        "RGB",
        unreal.MaterialProperty.MP_BASE_COLOR,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        normal,
        "RGB",
        unreal.MaterialProperty.MP_NORMAL,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness,
        "R",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )

    material.set_editor_property("use_material_attributes", False)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH)

    print(f"Created material: {MATERIAL_PATH}")
    print("Parameters: BaseColorTexture, NormalTexture, RoughnessTexture, DepthTexture, MaskTexture")


if __name__ == "__main__":
    main()
