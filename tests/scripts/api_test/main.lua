local DRAIN_MS = 339000

function _init()
    spawn()
end

function on_energy_drain(rw)
    local e = rw.energy or 0
    if e > 0 then
        rw.energy = e - 1
    end
    schedule(DRAIN_MS, "on_energy_drain")
end
