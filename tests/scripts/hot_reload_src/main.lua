energy = require("energy")

function on_spawn(api, rw)
    rw.energy = 10
    api.schedule(energy.DRAIN_INTERVAL, "energy.on_drain")
end
