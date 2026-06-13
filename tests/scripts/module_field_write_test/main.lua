mod = require("mod")

function _init()
    spawn()
    schedule(0, "on_start")
end

function on_start()
    mod.new_field = 42
end
