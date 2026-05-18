function _init()
    spawn()
    schedule(0, "on_start")
end

function on_start()
    cls(0xFF0000)
end
