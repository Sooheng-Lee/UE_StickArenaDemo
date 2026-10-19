"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
for name in ("Attack", "Defense", "Ammo"):
    task = unreal.AssetImportTask()
    task.filename = str(root / "Art" / "UI" / "Icons" / f"T_UI_{name}.png")
    task.destination_path = "/Game/UI/Icons"
    task.destination_name = f"T_UI_{name}"
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(f"/Game/UI/Icons/T_UI_{name}")
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Failed to import {name} HUD icon")
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("max_texture_size", 256)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.log(f"HUD icon imported: {texture.get_path_name()}")
