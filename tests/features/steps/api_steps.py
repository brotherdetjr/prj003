import json

from behave import when, then
from utils import get_screen, post, raw_request


# ---------------------------------------------------------------------------
# When
# ---------------------------------------------------------------------------

@when('I get state')
def step_get_state(context):
    context.state = post(context, {'cmd': 'get_state'})


@when('I get autotick')
def step_get_autotick(context):
    post(context, {'cmd': 'get_autotick'})


@when('I get stop_on_lua_error')
def step_get_stop_on_lua_error(context):
    post(context, {'cmd': 'get_stop_on_lua_error'})


@when('I set stop_on_lua_error to {value}')
def step_set_stop_on_lua_error(context, value):
    post(context, {'cmd': 'set_stop_on_lua_error', 'enabled': value.lower() == 'true'})


@when('I get wall clock')
def step_get_wall_clock(context):
    post(context, {'cmd': 'get_wall_clock'})


@when('I get the screen')
def step_get_screen(context):
    context.screen_png = get_screen(context)


@when('I spawn a character')
def step_spawn(context):
    context.state = post(context, {'cmd': 'spawn'})


@when('I advance {ticks:d} ticks')
def step_advance_ticks(context, ticks):
    post(context, {'cmd': 'advance_time', 'ticks': ticks})


@when('I advance to the next event')
def step_advance_next_event(context):
    post(context, {'cmd': 'advance_time', 'ticks': 0, 'stop_on_event': True})


@when('I set wall clock to {ts:d}')
def step_set_wall_clock(context, ts):
    post(context, {'cmd': 'set_wall_clock', 'now_unix_sec': ts})


@when('I poof the character')
def step_poof(context):
    post(context, {'cmd': 'poof'})


@when('I post command:')
def step_post_command_body(context):
    raw_request(context, 'POST', '/command', json=json.loads(context.text))


@when('I send a {method} request to "{path}"')
def step_send_request(context, method, path):
    raw_request(context, method, path)


@when('I POST raw body "{body}" to "{path}"')
def step_post_raw_body(context, body, path):
    raw_request(context, 'POST', path,
                data=body, headers={'Content-Type': 'application/json'})


# ---------------------------------------------------------------------------
# Then
# ---------------------------------------------------------------------------

@then('the response is ok')
def step_resp_ok(context):
    assert context.resp.get('ok') is True, context.resp


@then('the response has ok false')
def step_resp_not_ok(context):
    assert context.resp.get('ok') is False, \
        f'expected ok=false, got: {context.resp}'


@then('the error is "{msg}"')
def step_error_msg(context, msg):
    actual = context.resp.get('error')
    assert actual == msg, f'expected error={msg!r}, got {actual!r}'


@then('the HTTP status is {code:d}')
def step_http_status(context, code):
    assert context.http_status == code, \
        f'expected HTTP {code}, got {context.http_status}'
