import json
import os
import re
import shutil
import socket
import tempfile
import time
import uuid

from behave import given, then, when
from emu_steps import TEST_SCRIPTS_DIR
from utils import EMU

DEFAULT_SCRIPT = os.path.abspath(
    os.path.join(os.path.dirname(EMU), "../../scripts/main.lua")
)


# ---------------------------------------------------------------------------
# Given — start emu
# ---------------------------------------------------------------------------

@given("port {port:d} is occupied")
def step_occupy_port(context, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", port))
    sock.listen(1)
    context.occupied_sockets = getattr(context, "occupied_sockets", [])
    context.occupied_sockets.append(sock)


@given("a state file with now_tick {nowtick:d} and no character")
def step_state_file_empty(context, nowtick):
    _make_state_file(context, nowtick, character=None)


@given('a state file with a character "{char_id}" at energy {energy:d}')
def step_state_file_with_character(context, char_id, energy):
    character = {
        "id": char_id,
        "birth_unix_sec": 1775606400,
        "birth_tick": 0,
    }
    _make_state_file(context, 0, character=character, rw={"energy": energy})


@given('the hot-reload test directory is set up from "{src_name}"')
def step_setup_hot_reload(context, src_name):
    src_dir = os.path.join(TEST_SCRIPTS_DIR, src_name)
    tmp_dir = os.path.join(
        TEST_SCRIPTS_DIR, "tmp", "hot_reload_" + uuid.uuid4().hex[:8]
    )
    os.makedirs(tmp_dir)
    for fname in os.listdir(src_dir):
        if not fname.endswith(".lua.template"):
            shutil.copy(os.path.join(src_dir, fname), os.path.join(tmp_dir, fname))
    context.hot_reload_src_dir = src_dir
    context.hot_reload_dir = tmp_dir
    context.temp_dirs = getattr(context, "temp_dirs", []) + [tmp_dir]


@given('"{src}" is copied to "{dst}"')
@when('"{src}" is copied to "{dst}"')
def step_copy_file(context, src, dst):
    shutil.copy(
        os.path.join(context.hot_reload_src_dir, src),
        os.path.join(context.hot_reload_dir, dst),
    )
    if getattr(context, "emu_proc", None):
        time.sleep(0.3)  # wait for emu's 100 ms poll to detect the change


# ---------------------------------------------------------------------------
# Then
# ---------------------------------------------------------------------------


@then('instance_id is "{value}"')
def step_instance_id(context, value):
    actual = context.resp["ro"]["instance_id"]
    assert actual == value, f"expected instance_id={value!r}, got {actual!r}"


@then("instance_id is an 8-digit hex string")
def step_instance_id_random(context):
    actual = context.resp["ro"]["instance_id"]
    assert re.fullmatch(r"[0-9A-F]{8}", actual), (
        f"expected 8-digit hex string, got {actual!r}"
    )


@then("autotick is {value}")
def step_autotick(context, value):
    expected = value.lower() == "true"
    actual = context.resp["autotick"]
    assert actual is expected, f"expected autotick={expected}, got {actual}"


@then("the exit code is {code:d}")
def step_exit_code(context, code):
    actual = context.proc_result.returncode
    assert actual == code, f"expected exit code {code}, got {actual}"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_state_file(context, now_tick, character=None, rw=None):
    state = {
        "ro": {
            "instance_id": "DEADBEEF",
            "now_tick": now_tick,
            "now_unix_sec": 1775606400,
            "character": character,
        },
        "rw": rw or {},
        "scheduler": [],
    }
    f = tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False)
    json.dump(state, f)
    f.close()
    context.state_file = f.name
    context.temp_files = getattr(context, "temp_files", []) + [f.name]
