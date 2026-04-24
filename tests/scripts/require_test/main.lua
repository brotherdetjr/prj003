drain = require("drain")

function on_spawn(api, rw)
    rw.energy = 10
    api.schedule(5000, "drain.on_drain")
end
