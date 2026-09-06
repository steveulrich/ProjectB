"""Run in Unreal Editor with DevMap or Dorado open and PIE stopped.

Author base volumes around inspected team spawn clusters and synchronize the
legacy PlayerStartTag consumed by BreakawayGameMode with existing Lyra tags.
Saves only the current map. Repeated runs update named zones, not duplicate them.
"""
import unreal


def configure_current_map():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor.get_game_world() is not None:
        raise RuntimeError("Stop PIE before authoring map bases")
    world = editor.get_editor_world()
    layouts = {
        "L_BW_Dorado": [((-70, 3575, 362), (450, 300, 250)), ((5, -3510, 362), (450, 300, 250))],
        "L_BW_DevMap": [((-945, 120, 92), (350, 350, 250)), ((1300, 95, 92), (350, 350, 250))],
    }
    if world.get_name() not in layouts:
        raise RuntimeError("Open DevMap or Dorado before running this script")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    starts = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.LyraPlayerStart)]
    assignments = []
    for start in starts:
        tags = start.get_editor_property("start_point_tags").export_text()
        matches = [i for i in (0, 1) if f'SpawnPoint.Player.Team{i + 1}' in tags]
        if len(matches) != 1:
            raise RuntimeError(f"Ambiguous team on {start.get_actor_label()}: {tags}")
        assignments.append((start, matches[0]))
    if not all(any(team == i for _, team in assignments) for i in (0, 1)):
        raise RuntimeError("Both teams need authored starts")
    for start, team in assignments:
        start.set_editor_property("player_start_tag", f"Team{team + 1}Spawn")
    existing = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
    for team, (position, extent) in enumerate(layouts[world.get_name()]):
        label = f"BaseZone_Team{team + 1}"
        zone = existing.get(label)
        if zone is not None and not isinstance(zone, unreal.BwayBaseZone):
            raise RuntimeError(f"Name collision: {label}")
        if zone is None:
            zone = actors.spawn_actor_from_class(unreal.BwayBaseZone, unreal.Vector(*position))
        zone.set_actor_label(label)
        zone.set_actor_location(unreal.Vector(*position), False, True)
        zone.set_editor_property("team_index", team)
        zone.bounds.set_box_extent(unreal.Vector(*extent), False)
        zone.set_editor_property("bEnabled", True)
        print("BASE_AUTHORED", world.get_name(), label, position, extent)
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
        raise RuntimeError("Map save failed")
    print("BASE_START_TAGS", world.get_name(), [(s.get_actor_label(), t) for s, t in assignments])


configure_current_map()
del configure_current_map
