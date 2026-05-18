function _init(rw)
    rw.order = {}
    table.insert(rw.order, "_init")
    schedule(50, "on_myevent")
end

function on_myevent(rw)
    table.insert(rw.order, "on_myevent")
    schedule(100, "on_myevent")
end

function _update(rw)
    table.insert(rw.order, "_update")
end
