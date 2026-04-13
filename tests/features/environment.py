import os


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
