function on_spawn(rw)
    rw.energy = 10
    schedule(50, "on_error_event")
end

function on_error_event(rw)
    bad_global = 99
end
