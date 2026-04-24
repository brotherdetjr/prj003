local M = {}

M.DRAIN_INTERVAL = 9000

function M.on_drain(api, rw)
    local e = rw.energy or 0
    if e > 0 then rw.energy = e - 1 end
    api.schedule(M.DRAIN_INTERVAL, "energy.on_drain")
end

return M
