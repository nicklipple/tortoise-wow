#include "ElunaRuntime.h"

#include "Config/Config.h"
#include "ElunaBindings.h"
#include "ElunaErrorReporter.h"
#include "ElunaScriptLoader.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"

#include <memory>
#include <utility>

extern "C"
{
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace Eluna
{
    char RUNTIME_REGISTRY_KEY = 0;

    namespace
    {
        constexpr std::uint32_t PLAYER_CALLBACK_CATEGORY = 1;
        constexpr std::uint32_t PLAYER_EVENT_ON_LOGIN = 3;

        CallbackKey PlayerLoginCallbackKey()
        {
            return { PLAYER_CALLBACK_CATEGORY, PLAYER_EVENT_ON_LOGIN };
        }
    }

    Runtime::~Runtime()
    {
        Stop();
    }

    bool Runtime::IsOnOwnerThread() const
    {
        return _ownerThread != std::thread::id{} && _ownerThread == std::this_thread::get_id();
    }

    bool Runtime::Start()
    {
        if (_state)
        {
            if (!IsOnOwnerThread())
            {
                sLog.outError("[Eluna]: Lua state access was attempted from a non-owner thread.");
                return false;
            }
            return true;
        }

        std::thread::id const currentThread = std::this_thread::get_id();
        if (_ownerThread == std::thread::id{})
            _ownerThread = currentThread;
        else if (_ownerThread != currentThread)
        {
            sLog.outError("[Eluna]: Lua state startup was attempted from a non-owner thread.");
            return false;
        }

        if (_startAttempted)
            return false;
        _startAttempted = true;

        if (!sConfig.GetBoolDefault("Eluna.Enabled", false))
        {
            sLog.outString("[Eluna]: Disabled by configuration.");
            return false;
        }

        _state = luaL_newstate();
        if (!_state)
        {
            sLog.outError("[Eluna]: Could not create the Lua state.");
            return false;
        }

        luaL_openlibs(_state);
        lua_pushlightuserdata(_state, &RUNTIME_REGISTRY_KEY);
        lua_pushlightuserdata(_state, this);
        lua_rawset(_state, LUA_REGISTRYINDEX);
        Bindings::Register(_state);

        ScriptLoader loader;
        loader.Load(_state, _currentScript);
        sLog.outString("[Eluna]: Proof-of-concept state started.");
        return true;
    }

    void Runtime::Stop()
    {
        if (_ownerThread != std::thread::id{} && !IsOnOwnerThread())
        {
            sLog.outError("[Eluna]: Lua state shutdown was attempted from a non-owner thread.");
            return;
        }

        if (_state)
        {
            _callbacks.Clear(_state);
            _handles.InvalidateAll();
            lua_pushlightuserdata(_state, &RUNTIME_REGISTRY_KEY);
            lua_pushnil(_state);
            lua_rawset(_state, LUA_REGISTRYINDEX);
            lua_State* state = _state;
            _state = nullptr;
            lua_close(state);
        }

        _handles.InvalidateAll();
        _currentScript.clear();
        _ownerThread = std::thread::id{};
        _startAttempted = false;
    }

    void Runtime::OnLogin(Player* player)
    {
        if (!player || !Start() || !_state || !IsOnOwnerThread())
            return;

        std::uint64_t const guid = player->GetObjectGuid().GetRawValue();
        std::uint64_t const generation = _handles.Activate(guid);

        CallbackEntry callback;
        if (!_callbacks.Get(PlayerLoginCallbackKey(), callback))
            return;

        lua_rawgeti(_state, LUA_REGISTRYINDEX, callback.reference);
        lua_pushinteger(_state, PLAYER_EVENT_ON_LOGIN);
        PushPlayer(guid, generation);
        if (lua_pcall(_state, 2, 0, 0) != 0)
            ErrorReporter::Callback(_state, "Player login (event 3)", callback.source);
    }

    void Runtime::OnBeforeLogout(Player* player)
    {
        if (!player)
            return;

        if (!IsOnOwnerThread())
        {
            if (_ownerThread != std::thread::id{})
                sLog.outError("[Eluna]: Player handle invalidation was attempted from a non-owner thread.");
            return;
        }

        _handles.Invalidate(player->GetObjectGuid().GetRawValue());
    }

    bool Runtime::ReplacePlayerLoginCallback(int reference)
    {
        if (!_state || !IsOnOwnerThread())
            return false;

        std::string source = _currentScript.empty() ? "<runtime>" : _currentScript;
        _callbacks.Replace(_state, PlayerLoginCallbackKey(), reference, std::move(source));
        return true;
    }

    Player* Runtime::ResolvePlayer(ObjectHandle const& handle) const
    {
        if (!_state || !IsOnOwnerThread())
            return nullptr;
        if (static_cast<HandleType>(handle.type) != HandleType::Player)
            return nullptr;
        if (!_handles.IsCurrent(handle.guid, handle.generation))
            return nullptr;

        ObjectGuid const guid(handle.guid);
        if (!guid.IsPlayer())
            return nullptr;

        Player* player = ObjectAccessor::FindPlayer(guid);
        if (!player || player->IsDeleted() || !player->IsInWorld())
            return nullptr;
        if (player->GetObjectGuid().GetRawValue() != handle.guid)
            return nullptr;

        return player;
    }

    void Runtime::PushPlayer(std::uint64_t guid, std::uint64_t generation)
    {
        Bindings::PushPlayer(_state, guid, generation);
    }

    ElunaRuntime* GetRuntime(lua_State* state)
    {
        if (!state)
            return nullptr;

        lua_pushlightuserdata(state, &RUNTIME_REGISTRY_KEY);
        lua_rawget(state, LUA_REGISTRYINDEX);
        ElunaRuntime* runtime = lua_islightuserdata(state, -1)
            ? static_cast<ElunaRuntime*>(lua_touserdata(state, -1))
            : nullptr;
        lua_pop(state, 1);
        return runtime;
    }
}
