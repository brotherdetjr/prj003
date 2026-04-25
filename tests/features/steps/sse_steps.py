import json as json_module
import queue as queue_module
import threading
import time

import requests
from behave import given, then


@given('I subscribe to SSE events')
def step_subscribe_sse(context):
    q = queue_module.Queue()
    context.sse_events = q
    ready = threading.Event()

    def _reader():
        try:
            with requests.get(
                    f'http://localhost:{context.port}/events',
                    stream=True, timeout=10) as r:
                ready.set()
                buf = ''
                for chunk in r.iter_content(chunk_size=None, decode_unicode=True):
                    buf += chunk
                    while '\n\n' in buf:
                        block, buf = buf.split('\n\n', 1)
                        ev = {}
                        for line in block.splitlines():
                            if line.startswith('event:'):
                                ev['event'] = line[len('event:'):].strip()
                            elif line.startswith('data:'):
                                ev['data'] = line[len('data:'):].strip()
                        if ev:
                            q.put(ev)
        except Exception:
            ready.set()

    t = threading.Thread(target=_reader, daemon=True)
    t.start()
    ready.wait(timeout=3)


@then('I do not receive an SSE "{event}" event')
def step_no_sse_event(context, event):
    deadline = time.time() + 0.5
    while time.time() < deadline:
        try:
            ev = context.sse_events.get(timeout=0.1)
            assert ev.get('event') != event, \
                f'unexpectedly received SSE event {event!r}'
        except queue_module.Empty:
            continue


@then('I receive an SSE "{event}" event at now_tick {tick:d}')
def step_receive_sse_event(context, event, tick):
    deadline = time.time() + 3.0
    while time.time() < deadline:
        try:
            ev = context.sse_events.get(timeout=0.2)
            if ev.get('event') == event:
                data = json_module.loads(ev.get('data', '{}'))
                assert data.get('now_tick') == tick, \
                    f'expected now_tick={tick}, got {data}'
                return
        except queue_module.Empty:
            continue
    assert False, f'timed out waiting for SSE event {event!r} at now_tick {tick}'
