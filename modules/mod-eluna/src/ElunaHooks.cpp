#include "LuaEngine.h"
#include "BindingMap.h"
#include "HookHelpers.h"
#include "Hooks.h"

void Eluna::OnLogin(Player* player)
{
    using Key = EventKey<Hooks::PlayerEvents>;
    BindingMap<Key>* binding = GetBinding<Key>(Hooks::REGTYPE_PLAYER);
    Key key(Hooks::PLAYER_EVENT_ON_LOGIN);
    if (!binding || !binding->HasBindingsFor(key))
        return;

    HookPush(player);
    CallAllFunctions(binding, key);
}

void Eluna::OnLuaStateOpen()
{
}

void Eluna::OnLuaStateClose()
{
}

void Eluna::OnTimedEvent(int /*funcRef*/, uint32 /*delay*/, uint32 /*calls*/, WorldObject* /*object*/)
{
}
