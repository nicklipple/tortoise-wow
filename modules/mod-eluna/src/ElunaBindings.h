#pragma once

#include <cstdint>

struct lua_State;

namespace Eluna
{
    class Bindings final
    {
    public:
        static void Register(lua_State* state);
        static void PushPlayer(lua_State* state, std::uint64_t guid, std::uint64_t generation);
    };
}
