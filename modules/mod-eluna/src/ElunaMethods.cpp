#include "LuaEngine.h"
#include "ElunaTemplate.h"
#include "Hooks.h"
#include "Player.h"
#include "Chat.h"

extern "C"
{
#include "lauxlib.h"
}

namespace
{
    int RegisterPlayerEvent(Eluna* engine)
    {
        uint32 event = engine->CHECKVAL<uint32>(1);
        luaL_checktype(engine->L, 2, LUA_TFUNCTION);
        uint32 shots = engine->CHECKVAL<uint32>(3, 0);

        lua_pushvalue(engine->L, 2);
        int functionReference = luaL_ref(engine->L, LUA_REGISTRYINDEX);
        if (functionReference < 0)
            return luaL_argerror(engine->L, 2, "unable to make a ref to function");

        return engine->Register(Hooks::REGTYPE_PLAYER, 0, ObjectGuid(), 0, event, functionReference, shots);
    }

    int SendBroadcastMessage(Eluna* engine, Player* player)
    {
        const char* message = engine->CHECKVAL<const char*>(2);
        if (message[0] != '\0')
            ChatHandler(player->GetSession()).SendSysMessage(message);
        return 0;
    }

    ElunaRegister<> const GlobalMethods[] =
    {
        { "RegisterPlayerEvent", &RegisterPlayerEvent }
    };

    ElunaRegister<Player> const PlayerMethods[] =
    {
        { "SendBroadcastMessage", &SendBroadcastMessage }
    };
}

void RegisterMethods(Eluna* engine)
{
    ElunaTemplate<>::SetMethods(engine, GlobalMethods);
    ElunaTemplate<Player>::Register(engine, "Player");
    ElunaTemplate<Player>::SetMethods(engine, PlayerMethods);
}
