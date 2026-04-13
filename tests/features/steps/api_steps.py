import json

from behave import when, then
from utils import raw_request


# ---------------------------------------------------------------------------
# When
# ---------------------------------------------------------------------------

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
