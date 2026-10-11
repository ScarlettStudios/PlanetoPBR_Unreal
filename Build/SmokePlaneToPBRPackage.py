import json
import os
from pathlib import Path

import unreal


def verify_package(_delta):
    unreal.unregister_slate_post_tick_callback(tick_handle)
    try:
        variant = os.environ["PLANETOPBR_SMOKE_VARIANT"]
        plugin = Path(unreal.Paths.project_plugins_dir()) / "PlaneToPBR"
        descriptor = json.loads((plugin / "PlaneToPBR.uplugin").read_text(encoding="utf-8-sig"))
        expected = {"PlaneToPBR", "PlaneToPBREditor"}
        if variant == "MCP":
            expected.add("PlaneToPBRMCP")
            assert unreal.ToolsetRegistry.is_toolset_class_registered(unreal.PlaneToPBRToolset)
            schema = unreal.ToolsetRegistry.get_toolset_json_schema(unreal.PlaneToPBRToolset)
            for name in ("GeneratePBRPlaneFromImage", "GeneratePBRTextures", "ImportPBRTextures", "CreatePBRMaterial", "CreateDisplacedMesh"):
                assert name in schema, f"Missing tool schema: {name}"
            result = unreal.ToolsetRegistry.execute_tool("PlaneToPBRMCP.PlaneToPBRToolset", "GeneratePBRTextures", '{"imagePath":""}')
            assert result.get_editor_property("bIsComplete")
            assert "Image file" in result.get_editor_property("Error"), result.get_editor_property("Error")
        else:
            assert not hasattr(unreal, "PlaneToPBRToolset"), "Standard ZIP loaded MCP"
        assert {module["Name"] for module in descriptor["Modules"]} == expected
        marker = Path(unreal.Paths.project_saved_dir()) / "PlaneToPBRSmokePassed.txt"
        marker.parent.mkdir(parents=True, exist_ok=True)
        marker.write_text(f"{variant} passed", encoding="utf-8")
        unreal.log(f"PlaneToPBR {variant} packaged-plugin smoke test passed")
    finally:
        unreal.SystemLibrary.quit_editor()


tick_handle = unreal.register_slate_post_tick_callback(verify_package)