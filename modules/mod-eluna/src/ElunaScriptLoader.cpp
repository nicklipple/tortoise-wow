#include "ElunaScriptLoader.h"

#include "Config/Config.h"
#include "ElunaErrorReporter.h"
#include "Log.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <vector>

extern "C"
{
#include "lauxlib.h"
}

namespace Eluna
{
    namespace
    {
        namespace fs = std::filesystem;

        fs::path ResolveScriptPath()
        {
            fs::path scriptPath(sConfig.GetStringDefault("Eluna.ScriptPath", "lua_scripts"));
            std::error_code error;
            if (scriptPath.is_relative() && !fs::is_directory(scriptPath, error))
            {
                error.clear();
                fs::path const parentPath = fs::path("..") / scriptPath;
                if (fs::is_directory(parentPath, error))
                    scriptPath = parentPath;
            }

            return scriptPath;
        }
    }

    std::uint32_t ScriptLoader::Load(lua_State* state, std::string& currentScript) const
    {
        fs::path const scriptPath = ResolveScriptPath();
        std::error_code error;
        if (!fs::is_directory(scriptPath, error))
        {
            sLog.outError("[Eluna]: Script path `%s` does not exist.", scriptPath.generic_string().c_str());
            sLog.outString("[Eluna]: Proof-of-concept state started with no scripts.");
            return 0;
        }

        std::vector<fs::path> scripts;
        for (fs::recursive_directory_iterator it(scriptPath, error), end; it != end && !error; it.increment(error))
        {
            std::error_code fileError;
            if (it->is_regular_file(fileError) && !fileError && it->path().extension() == ".lua")
                scripts.push_back(it->path());
        }
        std::sort(scripts.begin(), scripts.end());

        std::uint32_t loaded = 0;
        for (fs::path const& script : scripts)
        {
            currentScript = script.generic_string();
            if (luaL_loadfile(state, currentScript.c_str()) != 0)
            {
                ErrorReporter::Script(state, "Could not compile", currentScript);
                currentScript.clear();
                continue;
            }

            if (lua_pcall(state, 0, 0, 0) != 0)
            {
                ErrorReporter::Script(state, "Could not execute", currentScript);
                currentScript.clear();
                continue;
            }

            ++loaded;
            currentScript.clear();
        }

        if (error)
            sLog.outError("[Eluna]: Could not scan script path `%s`.", scriptPath.generic_string().c_str());

        sLog.outString("[Eluna]: Loaded %u Lua script(s) from `%s`.", loaded, scriptPath.generic_string().c_str());
        return loaded;
    }
}
