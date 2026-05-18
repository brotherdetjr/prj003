drain = require("drain")

function _init(rw)
    spawn()
    rw.energy = 10
    schedule(5000, "drain.on_drain")
end
