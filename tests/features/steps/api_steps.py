import io
import json
import os
import re

from behave import then, when
from PIL import Image
from utils import FIXTURES_DIR, get_screen, post, raw_request


@when("I get state")
def step_get_state(context):
    context.state = post(context, {"cmd": "get_state"})


@when("I get autotick")
def step_get_autotick(context):
    post(context, {"cmd": "get_autotick"})


@when("I get stop_on_lua_error")
def step_get_stop_on_lua_error(context):
    post(context, {"cmd": "get_stop_on_lua_error"})


@when("I set stop_on_lua_error to {value}")
def step_set_stop_on_lua_error(context, value):
    post(context, {"cmd": "set_stop_on_lua_error", "enabled": value.lower() == "true"})


@when("I get wall clock")
def step_get_wall_clock(context):
    post(context, {"cmd": "get_wall_clock"})


@when("I get the screen")
def step_get_screen(context):
    context.screen_png = get_screen(context)


@when("I spawn a character")
def step_spawn(context):
    context.state = post(context, {"cmd": "spawn"})


@when("I advance {ticks:d} ticks")
def step_advance_ticks(context, ticks):
    post(context, {"cmd": "advance_time", "ticks": ticks})


@when("I advance to the next event")
def step_advance_next_event(context):
    post(context, {"cmd": "advance_time", "ticks": 0, "stop_on_event": True})


@when("I set wall clock to {ts:d}")
def step_set_wall_clock(context, ts):
    post(context, {"cmd": "set_wall_clock", "now_unix_sec": ts})


@when("I poof the character")
def step_poof(context):
    post(context, {"cmd": "poof"})


@when("I post command:")
def step_post_command_body(context):
    raw_request(context, "POST", "/command", json=json.loads(context.text))


@when('I send a {method} request to "{path}"')
def step_send_request(context, method, path):
    raw_request(context, method, path)


@when('I POST raw body "{body}" to "{path}"')
def step_post_raw_body(context, body, path):
    raw_request(
        context, "POST", path, data=body, headers={"Content-Type": "application/json"}
    )


@then("the response is ok")
def step_resp_ok(context):
    assert context.resp.get("ok") is True, context.resp


@then("the response has ok false")
def step_resp_not_ok(context):
    assert context.resp.get("ok") is False, f"expected ok=false, got: {context.resp}"


@then('the error is "{msg}"')
def step_error_msg(context, msg):
    actual = context.resp.get("error")
    assert actual == msg, f"expected error={msg!r}, got {actual!r}"


@then("the HTTP status is {code:d}")
def step_http_status(context, code):
    assert context.http_status == code, (
        f"expected HTTP {code}, got {context.http_status}"
    )


@then("now_tick is {value:d}")
def step_now_tick(context, value):
    resp = context.resp
    actual = resp["now_tick"] if "now_tick" in resp else resp["ro"]["now_tick"]
    assert actual == value, f"expected now_tick={value}, got {actual}"


@then("now_unix_sec is {value:d}")
def step_now_unix_sec(context, value):
    resp = context.resp
    actual = resp.get("now_unix_sec") or resp.get("ro", {}).get("now_unix_sec")
    assert actual == value, f"expected now_unix_sec={value}, got {actual}"


@then("there is no character")
def step_no_character(context):
    assert context.resp["ro"]["character"] is None, context.resp


@then('the character id is "{char_id}"')
def step_char_id(context, char_id):
    assert context.resp["ro"]["character"]["id"] == char_id, context.resp


@then("birth_unix_sec is {value:d}")
def step_birth_unix_sec(context, value):
    actual = context.resp["ro"]["character"]["birth_unix_sec"]
    assert actual == value, f"expected birth_unix_sec={value}, got {actual}"


@then("birth_tick is {value:d}")
def step_birth_tick(context, value):
    actual = context.resp["ro"]["character"]["birth_tick"]
    assert actual == value, f"expected birth_tick={value}, got {actual}"


@then("energy is {value:d}")
def step_energy(context, value):
    actual = context.state["rw"]["energy"]
    assert actual == value, f"expected rw.energy={value}, got {actual}"


@then("stopped_on_event is {value}")
def step_stopped_on_event(context, value):
    expected = value.lower() == "true"
    actual = context.resp["stopped_on_event"]
    assert actual is expected, f"expected stopped_on_event={expected}, got {actual}"


@then("stop_on_lua_error is {value}")
def step_stop_on_lua_error(context, value):
    expected = value.lower() == "true"
    actual = context.resp["stop_on_lua_error"]
    assert actual is expected, f"expected stop_on_lua_error={expected}, got {actual}"


@then('event is "{name}"')
def step_event_name(context, name):
    actual = context.resp.get("event")
    assert actual == name, f"expected event={name!r}, got {actual!r}"


@then('the scheduler has an "{event}" event at tick {tick:d}')
def step_scheduler_has_event(context, event, tick):
    sched = context.state.get("scheduler", [])
    for entry in sched:
        if entry.get("event") == event and int(entry.get("fire_at_ms", -1)) == tick:
            return
    assert False, f"no {event!r} event at tick {tick} in scheduler: {sched}"


@then("the scheduler has {count:d} event(s)")
def step_scheduler_count(context, count):
    sched = context.state.get("scheduler", [])
    assert len(sched) == count, (
        f"expected {count} event(s) in scheduler, got {len(sched)}: {sched}"
    )


@then("the scheduler is an empty array")
def step_scheduler_empty(context):
    sched = context.state.get("scheduler")
    assert sched == [], f"expected scheduler=[], got: {sched!r}"


@then("rw is an empty object")
def step_rw_empty(context):
    rw = context.state.get("rw")
    assert rw == {}, f"expected rw={{}}, got {rw!r}"


@then('rw field "{key}" is {value}')
def step_rw_field(context, key, value):
    actual = context.state.get("rw", {}).get(key)
    if value == "true":
        assert actual is True, f"expected rw.{key}=true, got {actual!r}"
    elif value == "false":
        assert actual is False, f"expected rw.{key}=false, got {actual!r}"
    else:
        assert actual == int(value), f"expected rw.{key}={value}, got {actual!r}"


@then('the screen matches fixture "{name}"')
def step_screen_matches_fixture(context, name):
    actual = Image.open(io.BytesIO(context.screen_png)).convert("RGB")
    expected = Image.open(os.path.join(FIXTURES_DIR, name)).convert("RGB")
    assert actual.size == expected.size, (
        f"size mismatch: {actual.size} != {expected.size}"
    )
    assert list(actual.getdata()) == list(expected.getdata()), "pixel data mismatch"


@then("autotick is {value}")
def step_autotick(context, value):
    expected = value.lower() == "true"
    actual = context.resp["autotick"]
    assert actual is expected, f"expected autotick={expected}, got {actual}"


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
