function _init()
    spawn()
end

function _update(rw)
    if not rw.done then
        rw.done = true
        schedule(0, string.rep("a", 64))
    end
end
