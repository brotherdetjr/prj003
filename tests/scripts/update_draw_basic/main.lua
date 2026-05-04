function _update(rw)
    rw.update_count = (rw.update_count or 0) + 1
end

function _draw(rw)
    rw.draw_count = (rw.draw_count or 0) + 1
    cls(0x00FF00)
end
