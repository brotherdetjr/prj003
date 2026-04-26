import os
import shutil


def after_scenario(context, scenario):
    proc = getattr(context, 'emu_proc', None)
    if proc:
        proc.terminate()
        proc.wait(timeout=5)
        context.emu_proc = None
    for path in getattr(context, 'temp_files', []):
        try:
            os.unlink(path)
        except OSError:
            pass
    context.temp_files = []
    for path in getattr(context, 'temp_dirs', []):
        shutil.rmtree(path, ignore_errors=True)
    context.temp_dirs = []
    for sock in getattr(context, 'occupied_sockets', []):
        sock.close()
    context.occupied_sockets = []
