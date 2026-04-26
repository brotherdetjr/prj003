myapp = {}
myapp.energy = require("energy")

function on_spawn(rw)
    rw.energy = 10
    schedule(5000, "myapp.energy.on_drain")
end
