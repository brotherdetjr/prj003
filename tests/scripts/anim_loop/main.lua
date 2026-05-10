anim("a").of("two_frames.png").loop(true)

function _draw()
    cls(0x000000)
    spr("two_frames.png", fr("a"), 0, 0)
end
