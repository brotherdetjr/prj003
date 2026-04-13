import json
import os
import re
import shlex
import subprocess
import tempfile

from behave import given, when, then
from utils import EMU, post, start_emu

ARGS_PORT = 17071


# ---------------------------------------------------------------------------
# Given — start emu
# ---------------------------------------------------------------------------

@given('emu starts with args "{args_str}"')
def step_emu_starts(context, args_str):
    start_emu(context, shlex.split(args_str), ARGS_PORT)


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
        'energy': energy,
    }
    _make_state_file(context, 0, character=character)


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
    actual = context.resp['state']['instance_id']
    assert actual == value, f'expected instance_id={value!r}, got {actual!r}'


@then('instance_id is an 8-digit hex string')
def step_instance_id_random(context):
    actual = context.resp['state']['instance_id']
    assert re.fullmatch(r'[0-9A-F]{8}', actual), \
        f'expected 8-digit hex string, got {actual!r}'


@then('autotick is {value}')
def step_autotick(context, value):
    expected = value.lower() == 'true'
    actual = context.resp['state']['autotick']
    assert actual is expected, f'expected autotick={expected}, got {actual}'


@then('the exit code is {code:d}')
def step_exit_code(context, code):
    actual = context.proc_result.returncode
    assert actual == code, f'expected exit code {code}, got {actual}'


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_state_file(context, now_tick, character=None):
    state = {
        'instance_id': 'DEADBEEF',
        'now_tick': now_tick,
        'now_unix_sec': 1775606400,
        'autotick': False,
        'character': character,
    }
    f = tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False)
    json.dump(state, f)
    f.close()
    context.state_file = f.name
    context.temp_files = getattr(context, 'temp_files', []) + [f.name]
