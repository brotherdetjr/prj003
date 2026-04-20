-- Main script for Gloxie

local DRAIN_MS = 339000

function on_energy_drain(api, rw)
    local e = rw.energy or 0
    if e > 0 then
        rw.energy = e - 1
    end
    api.schedule(DRAIN_MS, "on_energy_drain")
end

function on_spawn(api, rw)
    rw.energy = 255
    api.schedule(DRAIN_MS, "on_energy_drain")
end
