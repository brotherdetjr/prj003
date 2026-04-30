import os
import subprocess
import time

import requests

EMU = os.environ.get('EMU_BIN') or os.path.abspath(
    os.path.join(os.path.dirname(__file__), '../../../platform/pc/emu'))


def post(context, payload):
    r = requests.post(
        f'http://localhost:{context.port}/command',
        json=payload,
        timeout=5,
    )
    r.raise_for_status()
    context.resp = r.json()
    return context.resp


def raw_request(context, method, path, **kwargs):
    """Like post(), but works for any method/path and never raises on 4xx/5xx."""
    r = requests.request(
        method,
        f'http://localhost:{context.port}{path}',
        timeout=5,
        **kwargs,
    )
    context.http_status = r.status_code
    try:
        context.resp = r.json()
    except Exception:
        context.resp = {}
    return context.resp


def wait_for_emu(port, timeout=3.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            requests.post(f'http://localhost:{port}/command',
                          json={'cmd': 'get_state'}, timeout=0.3)
            return
        except Exception:
            time.sleep(0.1)
    raise RuntimeError(f'emu did not start on port {port} in time')


def run_emu(args, **kwargs):
    """Launch emu with --headless; returns the Popen object."""
    return subprocess.Popen([EMU] + args + ['--headless'], **kwargs)


def start_emu(context, extra_args, port):
    context.port = port
    context.emu_proc = run_emu(
        extra_args + [f'--port={port}'],
        stderr=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
    )
    wait_for_emu(port)
