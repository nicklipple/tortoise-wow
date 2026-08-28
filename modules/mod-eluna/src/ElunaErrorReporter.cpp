#include "ElunaErrorReporter.h"

#include "Log.h"

extern "C"
{
#include "lua.h"
}

namespace Eluna
{
    namespace
    {
        char const* ErrorMessage(lua_State* state)
        {
            char const* const error = state ? lua_tostring(state, -1) : nullptr;
            return error ? error : "unknown Lua error";
        }

        void PopError(lua_State* state)
        {
            if (state && lua_gettop(state) > 0)
                lua_pop(state, 1);
        }
    }

    void ErrorReporter::Script(lua_State* state, char const* operation, std::string const& source)
    {
        sLog.outError("[Eluna]: %s `%s`: %s", operation, source.c_str(), ErrorMessage(state));
        PopError(state);
    }

    void ErrorReporter::Callback(lua_State* state, std::string const& context, std::string const& source)
    {
        sLog.outError("[Eluna]: %s callback from `%s` failed: %s", context.c_str(), source.c_str(), ErrorMessage(state));
        PopError(state);
    }
}
