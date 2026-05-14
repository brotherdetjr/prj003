function _init()
    anim("a").of("two_frames.png")
end

function _draw(rw)
    cls(0x000000)
    if rw.show then
        aspr("a")
    end
end
