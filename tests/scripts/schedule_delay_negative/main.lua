function _init()
    spawn()
end

function _update(rw)
    if not rw.done then
        rw.done = true
        schedule(-1, "on_start")
    end
end

function on_start() end
