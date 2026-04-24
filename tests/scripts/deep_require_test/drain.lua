local M = {}

function M.on_drain(api, rw)
    local e = rw.energy or 0
    if e > 0 then
        rw.energy = e - 1
    end
    api.schedule(5000, "effects.drain.on_drain")
end

return M
