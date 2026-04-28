function on_spawn()
    schedule(50, "on_error_event")
end

function on_error_event()
    bad_global = 99
end
