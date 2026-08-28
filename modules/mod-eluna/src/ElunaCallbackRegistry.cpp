#include "ElunaCallbackRegistry.h"

extern "C"
{
#include "lauxlib.h"
}

#include <utility>

namespace Eluna
{
    void CallbackRegistry::Replace(lua_State* state, CallbackKey key, int reference, std::string source)
    {
        auto itr = _callbacks.find(key);
        if (itr != _callbacks.end())
        {
            if (itr->second.reference != LUA_NOREF && itr->second.reference != LUA_REFNIL)
                luaL_unref(state, LUA_REGISTRYINDEX, itr->second.reference);

            itr->second = { reference, std::move(source) };
            return;
        }

        _callbacks.emplace(key, CallbackEntry{ reference, std::move(source) });
    }

    void CallbackRegistry::Remove(lua_State* state, CallbackKey key)
    {
        auto itr = _callbacks.find(key);
        if (itr == _callbacks.end())
            return;

        if (itr->second.reference != LUA_NOREF && itr->second.reference != LUA_REFNIL)
            luaL_unref(state, LUA_REGISTRYINDEX, itr->second.reference);
        _callbacks.erase(itr);
    }

    void CallbackRegistry::Clear(lua_State* state)
    {
        if (state)
        {
            for (auto const& callback : _callbacks)
            {
                if (callback.second.reference != LUA_NOREF && callback.second.reference != LUA_REFNIL)
                    luaL_unref(state, LUA_REGISTRYINDEX, callback.second.reference);
            }
        }

        _callbacks.clear();
    }

    bool CallbackRegistry::Get(CallbackKey key, CallbackEntry& entry) const
    {
        auto const itr = _callbacks.find(key);
        if (itr == _callbacks.end())
            return false;

        entry = itr->second;
        return true;
    }
}
