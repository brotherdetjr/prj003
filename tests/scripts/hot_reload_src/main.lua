energy = require("energy")

function on_spawn(rw)
    rw.energy = 10
    schedule(energy.DRAIN_INTERVAL, "energy.on_drain")
end
