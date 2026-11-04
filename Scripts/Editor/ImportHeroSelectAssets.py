"""
Idempotent UE 5.8 Editor Python pipeline to import the Breakaway Hero Selection Menu
textures into /BreakawayCore/UI/Art/HeroSelect and compile/save:
- /BreakawayCore/UI/HeroSelect/W_HeroSelection
- /BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect
- /BreakawayCore/UI/HeroSelect/WBP_BW_HeroSlot

Follows AI_Planning/Runbooks/editor-pie.md and unreal-editor-automation:
- Deferred Slate post-tick job execution for asset import/save phases.
- Releases all local UObject references in try/finally + gc.collect().
"""

import gc
import glob
import json
import os
import pathlib
import time
import traceback
import unreal


PROJECT_ROOT = "E:/Unreal Projects/ProjectB"
PNG_SOURCE_DIR = f"{PROJECT_ROOT}/Saved/Temp/HeroSelectUI_PNGs"
DEST_ASSET_DIR = "/BreakawayCore/UI/Art/HeroSelect"
HERO_SELECT_WBPS = [
    "/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSlot",
    "/BreakawayCore/UI/HeroSelect/W_HeroSelection",
    "/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect",
]
JOB_STATUS_JSON = f"{PROJECT_ROOT}/Saved/Logs/heroselect-ui-import.json"

_job_handle = None
_job_request = None


def schedule_import_job():
    global _job_handle, _job_request
    if _job_handle is not None:
        return {"status": "already_active", "status_json": JOB_STATUS_JSON}
    os.makedirs(os.path.dirname(JOB_STATUS_JSON), exist_ok=True)
    _job_request = {
        "status": "scheduled",
        "started": time.time(),
    }
    pathlib.Path(JOB_STATUS_JSON).write_text(json.dumps(_job_request, indent=2), encoding="utf-8")
    _job_handle = unreal.register_slate_post_tick_callback(_run_import_job)
    return {"scheduled": True, "status_json": JOB_STATUS_JSON}


def _run_import_job(_delta_seconds):
    global _job_handle, _job_request
    unreal.unregister_slate_post_tick_callback(_job_handle)
    _job_handle = None
    req = _job_request
    _job_request = None
    req["status"] = "running"
    pathlib.Path(JOB_STATUS_JSON).write_text(json.dumps(req, indent=2), encoding="utf-8")
    try:
        result = _import_all_heroselect_assets()
        req["status"] = "complete"
        req["result"] = result
    except Exception:
        req["status"] = "failed"
        req["error"] = traceback.format_exc()
        unreal.log_error(req["error"])
    finally:
        gc.collect()
        req["elapsed_seconds"] = round(time.time() - req["started"], 3)
        pathlib.Path(JOB_STATUS_JSON).write_text(json.dumps(req, indent=2), encoding="utf-8")


def _import_all_heroselect_assets():
    asset_tools = None
    tasks = []
    imported_stems = []
    tex = None
    wbp = None
    try:
        png_files = sorted(glob.glob(f"{PNG_SOURCE_DIR}/*.png"))
        if not png_files:
            raise RuntimeError(f"No PNG files found in {PNG_SOURCE_DIR}")

        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        for src in png_files:
            stem = os.path.splitext(os.path.basename(src))[0]
            imported_stems.append(stem)
            t = unreal.AssetImportTask()
            t.filename = src.replace("\\", "/")
            t.destination_path = DEST_ASSET_DIR
            t.destination_name = stem
            t.replace_existing = True
            t.replace_existing_settings = False
            t.automated = True
            t.save = False
            tasks.append(t)

        asset_tools.import_asset_tasks(tasks)

        saved_count = 0
        for stem in imported_stems:
            tpath = f"{DEST_ASSET_DIR}/{stem}"
            tex = unreal.load_asset(tpath)
            if not tex:
                raise RuntimeError(f"Failed to load imported UI texture: {tpath}")
            tex.modify()
            tex.set_editor_property("srgb", True)
            tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
            tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
            tex.set_editor_property("never_stream", True)
            if unreal.EditorAssetLibrary.save_loaded_asset(tex, False):
                saved_count += 1
            tex = None

        # Populate, compile & save W_HeroSelection, WBP_BW_HeroSelect, and WBP_BW_HeroSlot via C++ builder command
        world = unreal.EditorLevelLibrary.get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, "bway.Editor.PopulateHeroSelectWidgetBlueprint")

        wbps_saved = {}
        for wbp_path in HERO_SELECT_WBPS:
            wbp = unreal.load_asset(wbp_path)
            saved_flag = False
            if wbp:
                unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
                saved_flag = bool(unreal.EditorAssetLibrary.save_loaded_asset(wbp, False))
                wbp = None
            wbps_saved[wbp_path] = saved_flag

        return {
            "imported_textures": len(imported_stems),
            "saved_textures": saved_count,
            "wbps_saved": wbps_saved,
        }
    finally:
        tex = None
        wbp = None
        tasks = None
        asset_tools = None
        gc.collect()
