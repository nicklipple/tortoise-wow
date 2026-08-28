#include "ElunaBindings.h"

#include "Chat.h"
#include "ElunaHandle.h"
#include "ElunaRuntime.h"
#include "Player.h"

#include <new>

extern "C"
{
#include "lauxlib.h"
#include "lua.h"
}

namespace Eluna
{
    namespace
    {
        constexpr int PLAYER_EVENT_ON_LOGIN = 3;
        char const* const OBJECT_METATABLE = "mod-eluna.Object";
        char const* const WORLD_OBJECT_METATABLE = "mod-eluna.WorldObject";
        char const* const UNIT_METATABLE = "mod-eluna.Unit";
        char const* const PLAYER_METATABLE = "mod-eluna.Player";

        int RegisterPlayerEvent(lua_State* state);
        int SendBroadcastMessage(lua_State* state);

        int ObjectHandleGc(lua_State* state)
        {
            // The payload is only scalar metadata. Lua owns and releases the
            // userdata storage; no Tortoise object is ever destroyed here.
            (void)state;
            return 0;
        }

        void RegisterMetatable(lua_State* state, char const* name, char const* parent)
        {
            luaL_newmetatable(state, name);
            lua_pushcfunction(state, &ObjectHandleGc);
            lua_setfield(state, -2, "__gc");
            lua_pushliteral(state, "Eluna object metatable");
            lua_setfield(state, -2, "__metatable");

            if (parent)
            {
                lua_newtable(state);
                luaL_getmetatable(state, parent);
                lua_getfield(state, -1, "__index");
                lua_remove(state, -2);
                lua_setmetatable(state, -2);
                lua_setfield(state, -2, "__index");
            }
            else
            {
                lua_newtable(state);
                lua_setfield(state, -2, "__index");
            }

            lua_pop(state, 1);
        }

        ObjectHandle* CheckPlayerHandle(lua_State* state)
        {
            ObjectHandle* handle = static_cast<ObjectHandle*>(luaL_checkudata(state, 1, PLAYER_METATABLE));
            if (static_cast<HandleType>(handle->type) != HandleType::Player)
                luaL_error(state, "invalid Player handle type");
            return handle;
        }

        int RegisterPlayerEvent(lua_State* state)
        {
            ElunaRuntime* runtime = GetRuntime(state);
            if (!runtime || !runtime->IsOnOwnerThread())
                return luaL_error(state, "Eluna state is unavailable or called from a non-owner thread");

            lua_Integer const event = luaL_checkinteger(state, 1);
            luaL_checktype(state, 2, LUA_TFUNCTION);
            if (event != PLAYER_EVENT_ON_LOGIN)
                return luaL_error(state, "this proof of concept only supports player login event 3");

            lua_pushvalue(state, 2);
            int const reference = luaL_ref(state, LUA_REGISTRYINDEX);
            if (!runtime->ReplacePlayerLoginCallback(reference))
            {
                luaL_unref(state, LUA_REGISTRYINDEX, reference);
                return luaL_error(state, "Eluna callback registry is unavailable");
            }

            return 0;
        }

        int SendBroadcastMessage(lua_State* state)
        {
            ElunaRuntime* runtime = GetRuntime(state);
            if (!runtime || !runtime->IsOnOwnerThread())
                return luaL_error(state, "Eluna state is unavailable or called from a non-owner thread");

            ObjectHandle* handle = CheckPlayerHandle(state);
            char const* message = luaL_checkstring(state, 2);
            Player* player = runtime->ResolvePlayer(*handle);
            if (!player)
                return luaL_error(state, "Player handle is no longer valid");

            if (player->GetSession() && message[0] != '\0')
                ChatHandler(player->GetSession()).SendSysMessage(message);
            return 0;
        }
    }

    void Bindings::Register(lua_State* state)
    {
        RegisterMetatable(state, OBJECT_METATABLE, nullptr);
        RegisterMetatable(state, WORLD_OBJECT_METATABLE, OBJECT_METATABLE);
        RegisterMetatable(state, UNIT_METATABLE, WORLD_OBJECT_METATABLE);
        RegisterMetatable(state, PLAYER_METATABLE, UNIT_METATABLE);

        luaL_getmetatable(state, PLAYER_METATABLE);
        lua_getfield(state, -1, "__index");
        lua_pushcfunction(state, &SendBroadcastMessage);
        lua_setfield(state, -2, "SendBroadcastMessage");
        lua_pop(state, 2);

        lua_pushcfunction(state, &RegisterPlayerEvent);
        lua_setglobal(state, "RegisterPlayerEvent");
    }

    void Bindings::PushPlayer(lua_State* state, std::uint64_t guid, std::uint64_t generation)
    {
        void* storage = lua_newuserdata(state, sizeof(ObjectHandle));
        ObjectHandle* handle = new (storage) ObjectHandle{
            guid,
            generation,
            static_cast<std::uint8_t>(HandleType::Player),
        };
        luaL_getmetatable(state, PLAYER_METATABLE);
        lua_setmetatable(state, -2);
    }
}
