Feature: Module path resolution — schedule prefix follows the global variable path

  Scenario Outline: <description>
    Given emu starts with test script "<script>" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    And energy is 10
    And the scheduler has an "<event>.on_drain" event at tick 5000

    When I advance to the next event
    Then the response is ok
    And stopped_on_event is true
    And event is "<event>.on_drain"

    When I get state
    Then energy is 9
    And the scheduler has an "<event>.on_drain" event at tick 10000

    Examples:
      | description                      | script                              | event         |
      | same name via init()             | module_init_test/main.lua           | energy        |
      | alias via init()                 | module_alias_test/main.lua          | nrg           |
      | alias via direct schedule        | module_alias_direct_test/main.lua   | nrg           |
      | nested path via init()           | module_nested_test/main.lua         | myapp.energy  |
      | nested path via direct schedule  | module_nested_direct_test/main.lua  | myapp.energy  |
