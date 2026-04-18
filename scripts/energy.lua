-- Energy drain script for Gloxie
-- Energy: 255 (full) → 0 (exhausted) over 24 hours.
-- One unit drains every 339,000 virtual milliseconds.
--
-- `gloxie` is passed as the first argument to every callback and lifecycle hook.

local DRAIN_MS = 339000

function on_energy_drain(gloxie)
    local e = gloxie.scripted.energy or 0
    if e > 0 then
        gloxie.scripted.energy = e - 1
    end
    gloxie.schedule(DRAIN_MS, "on_energy_drain")
end

function _on_spawn(gloxie)
    gloxie.scripted.energy = 255
    gloxie.schedule(DRAIN_MS, "on_energy_drain")
end
