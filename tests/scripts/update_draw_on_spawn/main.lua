function _init(rw)
    spawn()
    rw.spawned = true
end

function _update(rw)
    if rw.spawned then
        rw.spawn_before_update = true
    end
end
