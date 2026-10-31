"""Defer long editor jobs beyond the bridge request lifetime.

The local bridge captures stack data in a queued ticker and is unsafe when a
request times out. This schedules the actual work after the short call returns.
No world/actor wrappers are stored in globals. A single active job is allowed.
"""
import json
import pathlib
import runpy
import time
import traceback
import unreal

_handle = None
_request = None


def schedule(function, kwargs=None):
    global _handle, _request
    if _handle is not None:
        raise RuntimeError('Dorado editor job is already active')
    project = pathlib.Path(unreal.Paths.project_dir()).resolve()
    _request = {'function': function, 'kwargs': kwargs or {}, 'project': str(project), 'status': 'scheduled', 'started': time.time()}
    path = project / 'Saved/Logs/dorado-reconstruction-20261030/editor-job.json'
    path.write_text(json.dumps(_request, indent=2))
    _handle = unreal.register_slate_post_tick_callback(_run)
    print('DORADO_JOB_SCHEDULED', function)


def _run(delta):
    global _handle, _request
    unreal.unregister_slate_post_tick_callback(_handle)
    _handle = None
    request = _request
    _request = None
    project = pathlib.Path(request['project'])
    path = project / 'Saved/Logs/dorado-reconstruction-20261030/editor-job.json'
    request['status'] = 'running'
    path.write_text(json.dumps(request, indent=2))
    try:
        pipeline = runpy.run_path(str(project / 'SourceArt/Dorado/Scripts/import_dorado.py'))
        pipeline[request['function']](**request['kwargs'])
        request['status'] = 'complete'
    except Exception:
        request['status'] = 'failed'
        request['error'] = traceback.format_exc()
        unreal.log_error(request['error'])
    finally:
        request['elapsed_seconds'] = round(time.time() - request['started'], 3)
        path.write_text(json.dumps(request, indent=2))
        print('DORADO_JOB_FINISHED', request['function'], request['status'])
