function on_spawn(rw)
    rw.event_count = 0
    rw.update_count = 0
    rw.order_ok = true
    schedule(50, "on_myevent")
end

function on_myevent(rw)
    rw.event_count = rw.event_count + 1
    schedule(100, "on_myevent")
end

function _update(rw)
    rw.update_count = rw.update_count + 1
    if rw.event_count ~= rw.update_count then
        rw.order_ok = false
    end
end
