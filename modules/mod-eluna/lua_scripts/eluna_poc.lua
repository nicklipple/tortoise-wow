local function OnLogin(event, player)
    player:SendBroadcastMessage("Eluna POC: Lua login event received.")
end

RegisterPlayerEvent(3, OnLogin)
