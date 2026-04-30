Feature: Graphics

  Scenario: screen is black on startup
    Given emu starts with args "--noautotick"
    When I get the screen
    Then the screen matches fixture "black_368x448.png"

  Scenario: cls fills the screen on spawn
    Given emu starts with test script "cls_on_spawn/main.lua" and args "--noautotick"
    When I spawn a character
    And I get the screen
    Then the screen matches fixture "red_368x448.png"
