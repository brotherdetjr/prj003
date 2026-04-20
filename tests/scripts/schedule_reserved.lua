function _on_spawn(api, rw, ro)
    api.schedule(100, "_bad_event")
end
