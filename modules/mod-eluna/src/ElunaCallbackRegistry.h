#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

struct lua_State;

namespace Eluna
{
    struct CallbackKey
    {
        std::uint32_t category = 0;
        std::uint32_t event = 0;

        bool operator==(CallbackKey const& other) const
        {
            return category == other.category && event == other.event;
        }
    };

    struct CallbackKeyHash
    {
        std::size_t operator()(CallbackKey const& key) const
        {
            std::size_t const category = std::hash<std::uint32_t>{}(key.category);
            std::size_t const event = std::hash<std::uint32_t>{}(key.event);
            return category ^ (event + static_cast<std::size_t>(0x9e3779b9) + (category << 6) + (category >> 2));
        }
    };

    struct CallbackEntry
    {
        int reference = 0;
        std::string source;
    };

    class CallbackRegistry final
    {
    public:
        void Replace(lua_State* state, CallbackKey key, int reference, std::string source);
        void Remove(lua_State* state, CallbackKey key);
        void Clear(lua_State* state);
        bool Get(CallbackKey key, CallbackEntry& entry) const;

    private:
        std::unordered_map<CallbackKey, CallbackEntry, CallbackKeyHash> _callbacks;
    };
}
