import os
import shlex

from behave import given
from utils import start_emu

ARGS_PORT = 17071

TEST_SCRIPTS_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "../../scripts")
)


@given('emu starts with args "{args_str}"')
def step_emu_starts(context, args_str):
    start_emu(context, shlex.split(args_str), ARGS_PORT)


@given('emu starts with test script "{script_name}" and args "{args_str}"')
def step_emu_starts_with_test_script(context, script_name, args_str):
    script = os.path.join(TEST_SCRIPTS_DIR, script_name)
    start_emu(context, shlex.split(args_str) + [f"--script={script}"], ARGS_PORT)


@given('emu starts on port {port:d} with args "{args_str}"')
def step_emu_starts_on_port(context, port, args_str):
    start_emu(context, shlex.split(args_str), port)
