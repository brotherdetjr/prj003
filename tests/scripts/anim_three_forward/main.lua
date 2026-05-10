anim("a").of("three_frames.png")

function _draw()
    cls(0x000000)
    spr("three_frames.png", fr("a"), 0, 0)
end
