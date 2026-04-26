link1 = require("link1")
link2 = require("link2")

function on_spawn(rw)
    rw.energy = 10
    schedule(10000, "link1.drain.on_drain")
    schedule(11000, "link2.drain.on_drain")
end
