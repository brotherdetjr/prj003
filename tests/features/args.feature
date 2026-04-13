Feature: Command-line argument parsing

  # ---------------------------------------------------------------------------
  # Happy path — each flag does what it says
  # ---------------------------------------------------------------------------

  Scenario: --id sets the instance ID
    Given emu starts with args "--id=CAFEBABE --nowtick=0 --noautotick"
    When I get state
    Then instance_id is "CAFEBABE"

  Scenario: --nowtick sets the initial virtual clock
    Given emu starts with args "--nowtick=5000 --noautotick"
    When I get state
    Then now_tick is 5000

  Scenario: --wallclockutc sets the wall clock
    Given emu starts with args "--wallclockutc=2026-01-01T00:00:00 --noautotick"
    When I get wall clock
    Then now_unix_sec is 1767225600

  Scenario: --noautotick disables autotick
    Given emu starts with args "--nowtick=0 --noautotick"
    When I get state
    Then autotick is false

  Scenario: --port changes the listening port
    Given emu starts on port 17072 with args "--nowtick=0 --noautotick"
    When I get state
    Then the response is ok

  Scenario: --file loads now_tick and wall clock
    Given a state file with now_tick 9999 and no character
    And emu starts with that state file and args "--noautotick"
    When I get state
    Then now_tick is 9999
    And there is no character

  Scenario: --file restores a character
    Given a state file with a character "CAFEBABE" at scripted energy 200
    And emu starts with that state file and args "--noautotick"
    When I get state
    Then the character id is "CAFEBABE"
    And scripted energy is 200

  # ---------------------------------------------------------------------------
  # Defaults
  # ---------------------------------------------------------------------------

  Scenario: autotick is on by default
    Given emu starts with args "--nowtick=0"
    When I get state
    Then autotick is true

  Scenario: random instance ID is generated when --id is omitted
    Given emu starts with args "--nowtick=0 --noautotick"
    When I get state
    Then instance_id is an 8-digit hex string

  # ---------------------------------------------------------------------------
  # Flag combinations — explicit flags override values loaded from --file
  # ---------------------------------------------------------------------------

  Scenario: --nowtick overrides now_tick from --file
    Given a state file with now_tick 9999 and no character
    And emu starts with that state file and args "--nowtick=1234 --noautotick"
    When I get state
    Then now_tick is 1234

  Scenario: --wallclockutc overrides wall clock from --file
    Given a state file with now_tick 9999 and no character
    And emu starts with that state file and args "--wallclockutc=2026-01-01T00:00:00 --noautotick"
    When I get wall clock
    Then now_unix_sec is 1767225600

  # ---------------------------------------------------------------------------
  # Invalid input — process must exit, no server started
  # ---------------------------------------------------------------------------

  Scenario: --file with nonexistent path exits with an error
    When emu is invoked with args "--file=/nonexistent/state.json"
    Then the exit code is 1

  Scenario: --id with empty value exits with an error
    When emu is invoked with args "--id="
    Then the exit code is 1

  Scenario: --id with non-hex value exits with an error
    When emu is invoked with args "--id=!!!"
    Then the exit code is 1

  Scenario: --nowtick with empty value exits with an error
    When emu is invoked with args "--nowtick="
    Then the exit code is 1

  Scenario: --nowtick with non-numeric value exits with an error
    When emu is invoked with args "--nowtick=abc"
    Then the exit code is 1

  Scenario: --file with empty path exits with an error
    When emu is invoked with args "--file="
    Then the exit code is 1

  Scenario: --help exits with code 0
    When emu is invoked with args "--help"
    Then the exit code is 0

  Scenario: unknown argument exits with an error
    When emu is invoked with args "--bogus"
    Then the exit code is 1

  Scenario: malformed --wallclockutc exits with an error
    When emu is invoked with args "--wallclockutc=not-a-date"
    Then the exit code is 1
