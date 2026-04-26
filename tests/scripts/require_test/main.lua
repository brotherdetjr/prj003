drain = require("drain")

function on_spawn(rw)
    rw.energy = 10
    schedule(5000, "drain.on_drain")
end
