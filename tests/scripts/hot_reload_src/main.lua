energy = require("energy")

function _init(rw)
    spawn()
    rw.energy = 10
    schedule(energy.DRAIN_INTERVAL, "energy.on_drain")
end
