local M = {}

function M.on_drain(rw)
    local e = rw.energy or 0
    if e > 0 then rw.energy = e - 1 end
    schedule(5000, "on_drain")
end

return M
