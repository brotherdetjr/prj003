from behave import given, when, then
from utils import post, start_emu

PORT = 17070


# ---------------------------------------------------------------------------
# Given
# ---------------------------------------------------------------------------

@given('a running emu with id "{emu_id}" nowtick {nowtick:d} wallclock "{wallclock}" in manual-tick mode')
def step_start_emu(context, emu_id, nowtick, wallclock):
    start_emu(context, [
        f'--id={emu_id}',
        f'--nowtick={nowtick}',
        f'--wallclockutc={wallclock}',
        '--noautotick',
    ], PORT)


# ---------------------------------------------------------------------------
# When
# ---------------------------------------------------------------------------

@when('I get state')
def step_get_state(context):
    resp = post(context, {'cmd': 'get_state'})
    context.state = resp['state']


@when('I get wall clock')
def step_get_wall_clock(context):
    post(context, {'cmd': 'get_wall_clock'})


@when('I spawn a character')
def step_spawn(context):
    resp = post(context, {'cmd': 'spawn'})
    context.state = resp['state']


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


# ---------------------------------------------------------------------------
# Then
# ---------------------------------------------------------------------------

@then('the response is ok')
def step_resp_ok(context):
    assert context.resp.get('ok') is True, context.resp


@then('now_tick is {value:d}')
def step_now_tick(context, value):
    resp = context.resp
    actual = resp['now_tick'] if 'now_tick' in resp else resp['state']['now_tick']
    assert actual == value, f'expected now_tick={value}, got {actual}'


@then('now_unix_sec is {value:d}')
def step_now_unix_sec(context, value):
    resp = context.resp
    actual = (resp.get('now_unix_sec')
              or resp.get('state', {}).get('now_unix_sec'))
    assert actual == value, f'expected now_unix_sec={value}, got {actual}'


@then('there is no character')
def step_no_character(context):
    assert context.resp['state']['character'] is None, context.resp


@then('the character id is "{char_id}"')
def step_char_id(context, char_id):
    assert context.resp['state']['character']['id'] == char_id, context.resp


@then('birth_unix_sec is {value:d}')
def step_birth_unix_sec(context, value):
    actual = context.resp['state']['character']['birth_unix_sec']
    assert actual == value, f'expected birth_unix_sec={value}, got {actual}'


@then('birth_tick is {value:d}')
def step_birth_tick(context, value):
    actual = context.resp['state']['character']['birth_tick']
    assert actual == value, f'expected birth_tick={value}, got {actual}'


@then('energy is {value:d}')
def step_energy(context, value):
    actual = context.state['character']['energy']
    assert actual == value, f'expected energy={value}, got {actual}'


@then('stopped_on_event is {value}')
def step_stopped_on_event(context, value):
    expected = value.lower() == 'true'
    actual = context.resp['stopped_on_event']
    assert actual is expected, f'expected stopped_on_event={expected}, got {actual}'


@then('event is "{name}"')
def step_event_name(context, name):
    actual = context.resp.get('event')
    assert actual == name, f'expected event={name!r}, got {actual!r}'
