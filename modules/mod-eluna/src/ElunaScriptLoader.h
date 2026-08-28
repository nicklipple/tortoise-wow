#pragma once

#include <cstdint>
#include <string>

struct lua_State;

namespace Eluna
{
    class ScriptLoader final
    {
    public:
        std::uint32_t Load(lua_State* state, std::string& currentScript) const;
    };
}
