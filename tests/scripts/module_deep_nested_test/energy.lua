local M = {}

M.DRAIN_INTERVAL = 5000

function M.init(rw)
    rw.energy = 10
    schedule(M.DRAIN_INTERVAL, "on_drain")
end

function M.on_drain(rw)
    local e = rw.energy or 0
    if e > 0 then rw.energy = e - 1 end
    schedule(M.DRAIN_INTERVAL, "on_drain")
end

return M
