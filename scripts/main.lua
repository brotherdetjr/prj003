-- Energy drain script for Gloxie
-- Energy: 255 (full) → 0 (exhausted) over 24 hours.
-- One unit drains every 339,000 virtual milliseconds.

local DRAIN_MS = 339000

function on_energy_drain(ro, rw, api)
    local e = rw.energy or 0
    if e > 0 then
        rw.energy = e - 1
    end
    api.schedule(DRAIN_MS, "on_energy_drain")
end

function _on_spawn(ro, rw, api)
    rw.energy = 255
    api.schedule(DRAIN_MS, "on_energy_drain")
end
