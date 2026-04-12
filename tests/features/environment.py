def after_scenario(context, scenario):
    proc = getattr(context, 'emu_proc', None)
    if proc:
        proc.terminate()
        proc.wait(timeout=5)
        context.emu_proc = None
