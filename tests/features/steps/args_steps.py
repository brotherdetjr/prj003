import json
import os
import re
import shlex
import shutil
import subprocess
import tempfile
import time
import uuid

from behave import given, when, then
from utils import EMU, post, start_emu

ARGS_PORT = 17071


# ---------------------------------------------------------------------------
# Given — start emu
# ---------------------------------------------------------------------------

DEFAULT_SCRIPT = os.path.abspath(
    os.path.join(os.path.dirname(EMU), '../../scripts/main.lua'))

TEST_SCRIPTS_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '../../scripts'))


@given('emu starts with args "{args_str}"')
def step_emu_starts(context, args_str):
    start_emu(context, shlex.split(args_str), ARGS_PORT)


@given('emu starts with args "{args_str}" and the default script')
def step_emu_starts_with_default_script(context, args_str):
    start_emu(context,
              shlex.split(args_str) + [f'--script={DEFAULT_SCRIPT}'],
              ARGS_PORT)


@given('emu starts with test script "{script_name}" and args "{args_str}"')
def step_emu_starts_with_test_script(context, script_name, args_str):
    script = os.path.join(TEST_SCRIPTS_DIR, script_name)
    start_emu(context, shlex.split(args_str) + [f'--script={script}'], ARGS_PORT)


@given('emu starts on port {port:d} with args "{args_str}"')
def step_emu_starts_on_port(context, port, args_str):
    start_emu(context, shlex.split(args_str), port)


@given('a state file with now_tick {nowtick:d} and no character')
def step_state_file_empty(context, nowtick):
    _make_state_file(context, nowtick, character=None)


@given('a state file with a character "{char_id}" at energy {energy:d}')
def step_state_file_with_character(context, char_id, energy):
    character = {
        'id': char_id,
        'birth_unix_sec': 1775606400,
        'birth_tick': 0,
    }
    _make_state_file(context, 0, character=character, rw={'energy': energy})


@given('the hot-reload test directory is set up from "{src_name}"')
def step_setup_hot_reload(context, src_name):
    src_dir = os.path.join(TEST_SCRIPTS_DIR, src_name)
    tmp_dir = os.path.join(TEST_SCRIPTS_DIR, 'tmp',
                           'hot_reload_' + uuid.uuid4().hex[:8])
    os.makedirs(tmp_dir)
    for fname in os.listdir(src_dir):
        if not fname.endswith('.lua.template'):
            shutil.copy(os.path.join(src_dir, fname), os.path.join(tmp_dir, fname))
    context.hot_reload_src_dir = src_dir
    context.hot_reload_dir     = tmp_dir
    context.temp_dirs = getattr(context, 'temp_dirs', []) + [tmp_dir]


@given('emu starts with the hot-reload test script and args "{args_str}"')
def step_emu_hot_reload_script(context, args_str):
    script = os.path.join(context.hot_reload_dir, 'main.lua')
    start_emu(context, shlex.split(args_str) + [f'--script={script}'], ARGS_PORT)


@given('"{src}" is copied to "{dst}"')
@when('"{src}" is copied to "{dst}"')
def step_copy_file(context, src, dst):
    shutil.copy(
        os.path.join(context.hot_reload_src_dir, src),
        os.path.join(context.hot_reload_dir, dst),
    )
    if getattr(context, 'emu_proc', None):
        time.sleep(0.3)   # wait for emu's 100 ms poll to detect the change


@given('emu starts with that state file and args "{args_str}"')
def step_emu_starts_with_file(context, args_str):
    start_emu(
        context,
        shlex.split(args_str) + [f'--file={context.state_file}'],
        ARGS_PORT,
    )


# ---------------------------------------------------------------------------
# When — run without expecting a server
# ---------------------------------------------------------------------------

@when('emu is invoked with args "{args_str}"')
def step_emu_invoked(context, args_str):
    context.proc_result = subprocess.run(
        [EMU] + shlex.split(args_str),
        capture_output=True,
        timeout=5,
    )


# ---------------------------------------------------------------------------
# Then
# ---------------------------------------------------------------------------

@then('instance_id is "{value}"')
def step_instance_id(context, value):
    actual = context.resp['ro']['instance_id']
    assert actual == value, f'expected instance_id={value!r}, got {actual!r}'


@then('instance_id is an 8-digit hex string')
def step_instance_id_random(context):
    actual = context.resp['ro']['instance_id']
    assert re.fullmatch(r'[0-9A-F]{8}', actual), \
        f'expected 8-digit hex string, got {actual!r}'


@then('autotick is {value}')
def step_autotick(context, value):
    expected = value.lower() == 'true'
    actual = context.resp['autotick']
    assert actual is expected, f'expected autotick={expected}, got {actual}'


@then('the exit code is {code:d}')
def step_exit_code(context, code):
    actual = context.proc_result.returncode
    assert actual == code, f'expected exit code {code}, got {actual}'


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_state_file(context, now_tick, character=None, rw=None):
    state = {
        'ro': {
            'instance_id': 'DEADBEEF',
            'now_tick': now_tick,
            'now_unix_sec': 1775606400,
            'character': character,
        },
        'rw': rw or {},
        'scheduler': [],
    }
    f = tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False)
    json.dump(state, f)
    f.close()
    context.state_file = f.name
    context.temp_files = getattr(context, 'temp_files', []) + [f.name]
