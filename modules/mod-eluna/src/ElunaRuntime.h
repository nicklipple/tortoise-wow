#pragma once

#include "ElunaCallbackRegistry.h"
#include "ElunaHandle.h"
#include "ElunaHandleRegistry.h"

#include <cstdint>
#include <string>
#include <thread>

struct lua_State;
class Player;

namespace Eluna
{
    // A lightuserdata key keeps trusted runtime state out of the string-keyed
    // registry namespace visible to Lua scripts.
    extern char RUNTIME_REGISTRY_KEY;

    class Runtime final
    {
    public:
        ~Runtime();

        bool Start();
        void Stop();
        bool IsStarted() const { return _state != nullptr; }
        bool IsOnOwnerThread() const;

        void OnLogin(Player* player);
        void OnBeforeLogout(Player* player);

        bool ReplacePlayerLoginCallback(int reference);
        Player* ResolvePlayer(ObjectHandle const& handle) const;

    private:
        void PushPlayer(std::uint64_t guid, std::uint64_t generation);

        lua_State* _state = nullptr;
        std::thread::id _ownerThread;
        bool _startAttempted = false;
        std::string _currentScript;
        CallbackRegistry _callbacks;
        HandleRegistry _handles;
    };

    using ElunaRuntime = Runtime;

    ElunaRuntime* GetRuntime(lua_State* state);
}
