function _update(rw)
    rw.update_count = (rw.update_count or 0) + 1
    rw.colour = rw.update_count % 2 == 1 and 0xFF0000 or 0x00FF00
end

function _draw(rw)
    cls(rw.colour or 0x000000)
end
