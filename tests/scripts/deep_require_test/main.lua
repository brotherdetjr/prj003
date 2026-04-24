effects = require("effects")

function on_spawn(api, rw)
    rw.energy = 10
    api.schedule(5000, "effects.drain.on_drain")
end
