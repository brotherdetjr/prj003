myapp = {}
myapp.energy = require("energy")

function _init(rw)
    spawn()
    rw.energy = 10
    schedule(5000, "myapp.energy.on_drain")
end
