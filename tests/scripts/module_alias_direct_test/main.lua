nrg = require("energy")

function _init(rw)
    spawn()
    rw.energy = 10
    schedule(5000, "nrg.on_drain")
end
