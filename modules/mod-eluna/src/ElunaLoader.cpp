#include "ElunaLoader.h"
#include "ElunaConfig.h"
#include "ElunaCompat.h"

#include <algorithm>
#include <filesystem>

extern "C"
{
#include "lauxlib.h"
#include "lualib.h"
}

namespace
{
    namespace fs = std::filesystem;
}

ElunaLoader::ElunaLoader() : m_cacheState(SCRIPT_CACHE_NONE)
{
}

ElunaLoader* ElunaLoader::instance()
{
    static ElunaLoader loader;
    return &loader;
}

ElunaLoader::~ElunaLoader()
{
    if (m_reloadThread.joinable())
        m_reloadThread.join();
}

int ElunaLoader::LoadBytecodeChunk(lua_State* /*L*/, uint8* bytes, size_t len, BytecodeBuffer* buffer)
{
    buffer->insert(buffer->end(), bytes, bytes + len);
    return 0;
}

void ElunaLoader::LoadScripts()
{
    if (m_cacheState == SCRIPT_CACHE_LOADING || m_cacheState == SCRIPT_CACHE_READY)
        return;

    m_cacheState = SCRIPT_CACHE_LOADING;
    m_scriptCache.clear();
    m_requirePath.clear();
    m_requirecPath.clear();

    fs::path scriptPath(sElunaConfig->GetConfig(CONFIG_ELUNA_SCRIPT_PATH));
    std::error_code error;
    lua_State* lua = luaL_newstate();
    if (!lua)
    {
        ELUNA_LOG_ERROR("[Eluna]: Could not create the script compiler state.");
        m_cacheState = SCRIPT_CACHE_NONE;
        return;
    }

    luaL_openlibs(lua);
    if (fs::is_directory(scriptPath, error))
    {
        for (fs::recursive_directory_iterator it(scriptPath, error), end; it != end && !error; it.increment(error))
        {
            if (!it->is_regular_file(error) || it->path().extension() != ".lua")
                continue;

            LuaScript script;
            script.fileext = ".lua";
            script.filename = it->path().stem().generic_string();
            script.filepath = it->path().generic_string();
            script.modulepath = it->path().parent_path().generic_string();
            script.mapId = -1;

            if (luaL_loadfile(lua, script.filepath.c_str()) != 0)
            {
                ELUNA_LOG_ERROR("[Eluna]: Could not compile `%s`: %s", script.filepath.c_str(), lua_tostring(lua, -1));
                lua_pop(lua, 1);
                continue;
            }

            if (lua_dump(lua, reinterpret_cast<lua_Writer>(&ElunaLoader::LoadBytecodeChunk), &script.bytecode) != 0)
            {
                ELUNA_LOG_ERROR("[Eluna]: Could not cache `%s`.", script.filepath.c_str());
                lua_pop(lua, 1);
                continue;
            }

            lua_pop(lua, 1);
            m_scriptCache.push_back(std::move(script));
        }
    }
    else
    {
        ELUNA_LOG_ERROR("[Eluna]: Script path `%s` does not exist.", scriptPath.generic_string().c_str());
    }

    lua_close(lua);
    std::sort(m_scriptCache.begin(), m_scriptCache.end(), [](LuaScript const& left, LuaScript const& right)
    {
        return left.filepath < right.filepath;
    });

    std::string scriptRoot = scriptPath.generic_string();
    m_requirePath = scriptRoot + "/?.lua;";
    m_requirecPath = scriptRoot + "/?.so;";
    m_cacheState = SCRIPT_CACHE_READY;
    ELUNA_LOG_INFO("[Eluna]: Cached %u Lua script(s).", uint32(m_scriptCache.size()));
}

void ElunaLoader::ReloadElunaForMap(int /*mapId*/)
{
    // Reload support is intentionally outside this proof of concept.
}
