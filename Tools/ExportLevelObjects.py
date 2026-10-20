"""Export saved Main map data without modifying the map. See Config/MapData/README.md."""
import json
from pathlib import Path

import unreal


MAP = "/Game/Maps/Main"
TILE_FOLDER = "Arena/Floor/Tiles"
CLEARANCE = 2.0
ROOT = Path(unreal.Paths.project_dir())


def vector(value):
    return {axis: round(getattr(value, axis), 6) for axis in ("x", "y", "z")}


def rotation(value):
    return {axis: round(getattr(value, axis), 6) for axis in ("pitch", "yaw", "roll")}


def identity(actor):
    return {
        "actor_name": actor.get_name(),
        "actor_label": actor.get_actor_label(),
        "actor_path": actor.get_path_name(),
        "folder": str(actor.get_folder_path()),
    }


def export():
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP):
        raise RuntimeError("Could not load saved Main map")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    tiles = sorted(
        (a for a in actors if str(a.get_folder_path()) == TILE_FOLDER
         or str(a.get_folder_path()).startswith(TILE_FOLDER + "/")),
        key=lambda a: a.get_actor_label(),
    )
    if not tiles:
        raise RuntimeError("No Tiles found; refusing to export an unrestricted spawn area")
    # Commandlet worlds do not provide runtime scene queries. Use conservative
    # world bounds here and require collision/support validation at spawn time.
    obstacles = []
    for actor in actors:
        if actor in tiles:
            continue
        if not actor.get_components_by_class(unreal.StaticMeshComponent):
            continue
        center, extent = actor.get_actor_bounds(True)
        if extent.x > 0 and extent.y > 0 and extent.z > 0:
            obstacles.append((actor, center, extent))
    chest_mesh = unreal.load_asset("/Game/Maps/StaticMeshes/SM_TreasureBox")
    bounds = chest_mesh.get_bounds()
    half = bounds.box_extent
    origin = bounds.origin
    bottom = origin.z - half.z
    points, excluded = [], []
    for tile in tiles:
        center, extent = tile.get_actor_bounds(False)
        mesh = tile.get_component_by_class(unreal.StaticMeshComponent)
        tilt = tile.get_actor_rotation()
        component_tilt = mesh.get_editor_property("relative_rotation") if mesh else None
        # These saved Tiles are upright hexagonal SM_Ground meshes. Reject other
        # geometry rather than treating an arbitrary bounds top as a surface.
        if (not mesh or not mesh.get_editor_property("static_mesh")
                or mesh.get_editor_property("static_mesh").get_name() != "SM_Ground"
                or abs(tilt.pitch) > 0.001 or abs(tilt.roll) > 0.001
                or abs(component_tilt.pitch) > 0.001 or abs(component_tilt.roll) > 0.001
                or min(tile.get_actor_scale3d().to_tuple()) <= 0
                or min(mesh.get_editor_property("relative_scale3d").to_tuple()) <= 0):
            excluded.append(dict(identity(tile), reason="unsupported_tile_geometry"))
            continue
        surface = unreal.Vector(center.x, center.y, center.z + extent.z)
        # A circle inside the hex tile supports the complete chest footprint.
        lower, upper = mesh.get_local_bounds()
        component_scale = mesh.get_world_scale()
        inradius = min(abs(upper.y), abs(lower.y)) * min(abs(component_scale.x), abs(component_scale.y))
        footprint_radius = ((abs(origin.x) + half.x + CLEARANCE) ** 2
                            + (abs(origin.y) + half.y + CLEARANCE) ** 2) ** 0.5
        if footprint_radius > inradius:
            excluded.append(dict(identity(tile), reason="chest_footprint_exceeds_tile_inradius"))
            continue
        position = unreal.Vector(surface.x, surface.y, surface.z - bottom + CLEARANCE)
        box_center = position + origin
        blockers = [actor.get_name() for actor, obstacle_center, obstacle_extent in obstacles
                    if abs(box_center.x - obstacle_center.x) <= half.x + CLEARANCE + obstacle_extent.x
                    and abs(box_center.y - obstacle_center.y) <= half.y + CLEARANCE + obstacle_extent.y
                    and abs(box_center.z - obstacle_center.z) <= half.z + CLEARANCE + obstacle_extent.z]
        if blockers:
            excluded.append(dict(identity(tile), reason="overlaps_obstacle_bounds", blocking_actors=blockers))
            continue
        points.append(dict(identity(tile), id=tile.get_actor_label(),
                           surface_position=vector(surface), position=vector(position),
                           rotation={"pitch": 0, "yaw": 0, "roll": 0},
                           scale={"x": 1, "y": 1, "z": 1}))

    towers = []
    for actor in sorted((a for a in actors if isinstance(a, unreal.SACrystalTower)),
                        key=lambda a: a.get_actor_label()):
        mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
        light = actor.get_component_by_class(unreal.PointLightComponent)
        towers.append(dict(identity(actor),
            class_path=actor.get_class().get_path_name(),
            position=vector(actor.get_actor_location()),
            rotation=rotation(actor.get_actor_rotation()), scale=vector(actor.get_actor_scale3d()),
            mesh=mesh.get_editor_property("static_mesh").get_path_name(),
            mesh_relative_location=vector(mesh.get_editor_property("relative_location")),
            mesh_relative_rotation=rotation(mesh.get_editor_property("relative_rotation")),
            mesh_relative_scale=vector(mesh.get_editor_property("relative_scale3d")),
            materials=[m.get_path_name() if m else None for m in mesh.get_materials()],
            hit_effect={
                "duration_seconds": actor.get_editor_property("effect_duration"),
                "glow_strength": actor.get_editor_property("glow_strength"),
                "light_intensity": actor.get_editor_property("light_intensity"),
                "material_parameter": "CrystalGlowStrength",
                "fade": "(1 - clamp(elapsed / duration, 0, 1))^2",
                "light_relative_position": vector(light.get_editor_property("relative_location")),
                "light_color_srgb": {
                    axis: getattr(light.get_editor_property("light_color"), axis)
                    for axis in ("r", "g", "b", "a")},
                "light_attenuation_radius_cm": light.get_editor_property("attenuation_radius"),
            }))
    if not points or not towers:
        raise RuntimeError("Spawn points or towers missing; export aborted")
    data = {
        "schema_version": 1,
        "map": MAP,
        "source": "saved_map",
        "coordinate_system": {"space": "Unreal world", "unit": "cm", "up_axis": "Z", "rotation_unit": "degrees"},
        "treasure_box": {
            "class_path": "/Script/UE_StickArenaDemo.SATreasureBox",
            "mesh": chest_mesh.get_path_name(),
            "spawn_rules": {
                "tiles_only": True,
                "allowed_tile_folder": TILE_FOLDER,
                "selection": "random_without_replacement_from_spawn_points",
                "allow_arbitrary_xy": False,
                "fallback_when_no_valid_point": "skip_spawn",
                "max_boxes_per_point": 1,
                "surface_clearance_cm": CLEARANCE,
                "require_runtime_collision_check": True,
                "require_runtime_support_on_exported_tile": True,
                "trace_channel": "Visibility",
                "trace_complex": False,
                "export_validation": "upright_hex_tile_surface_and_conservative_obstacle_bounds",
            },
            "tile_count": len(tiles),
            "spawn_point_count": len(points),
            "spawn_points": points,
            "excluded_tiles": excluded,
        },
        "crystal_tower_count": len(towers),
        "crystal_towers": towers,
    }
    output = ROOT / "Config" / "MapData" / "Main_LevelObjects.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(data, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    unreal.log(f"LEVEL_OBJECTS_EXPORTED: {len(points)}/{len(tiles)} tile spawn points, {len(towers)} towers -> {output}")


export()
