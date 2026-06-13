function _init()
    spawn()
end

function _update(rw)
    if not rw.done then
        rw.done = true
        for i = 1, 65 do
            schedule(1000, "on_event")
        end
    end
end

function on_event() end
