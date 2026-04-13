-- Energy drain script for Gloxie
-- Energy: 255 (full) → 0 (exhausted) over 24 hours.
-- One unit drains every 339,000 virtual milliseconds.

local DRAIN_MS = 339000

local function do_drain()
    local e = gloxie.scripted.energy or 0
    if e > 0 then
        gloxie.scripted.energy = e - 1
    end
    -- Reschedule regardless so the event keeps firing (even at 0)
    gloxie.schedule(DRAIN_MS, do_drain, "energy_drain")
end

function on_spawn()
    gloxie.scripted.energy = 255
    gloxie.schedule(DRAIN_MS, do_drain, "energy_drain")
end

function on_restore()
    -- Re-queue one drain cycle from now so energy keeps ticking
    gloxie.schedule(DRAIN_MS, do_drain, "energy_drain")
end

function on_poof()
    -- Nothing to clean up; scheduler is cleared by world_poof_character
end
