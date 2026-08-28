#pragma once

#include <string>

struct lua_State;

namespace Eluna
{
    class ErrorReporter final
    {
    public:
        static void Script(lua_State* state, char const* operation, std::string const& source);
        static void Callback(lua_State* state, std::string const& context, std::string const& source);
    };
}
