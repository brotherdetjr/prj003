nrg = require("energy")

function on_spawn(rw)
    rw.energy = 10
    schedule(5000, "nrg.on_drain")
end
