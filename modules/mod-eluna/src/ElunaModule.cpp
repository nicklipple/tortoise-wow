#include "ElunaModule.h"

#include "Chat.h"
#include "Config/Config.h"
#include "Log.h"
#include "Player.h"
#include "ScriptObjects.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

extern "C"
{
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace
{
    namespace fs = std::filesystem;

    constexpr int PLAYER_EVENT_ON_LOGIN = 3;
    char const* const ENGINE_REGISTRY_KEY = "mod-eluna.poc.engine";
    char const* const PLAYER_METATABLE = "mod-eluna.Player";

    struct PlayerHandle
    {
        Player* player;
    };

    class ElunaPoc;

    ElunaPoc* GetEngine(lua_State* state)
    {
        lua_getfield(state, LUA_REGISTRYINDEX, ENGINE_REGISTRY_KEY);
        ElunaPoc* engine = static_cast<ElunaPoc*>(lua_touserdata(state, -1));
        lua_pop(state, 1);
        return engine;
    }

    int RegisterPlayerEvent(lua_State* state);
    int SendBroadcastMessage(lua_State* state);

    class ElunaPoc
    {
    public:
        ~ElunaPoc()
        {
            Stop();
        }

        bool Start()
        {
            if (_state)
                return true;

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
            lua_pushlightuserdata(_state, this);
            lua_setfield(_state, LUA_REGISTRYINDEX, ENGINE_REGISTRY_KEY);

            luaL_newmetatable(_state, PLAYER_METATABLE);
            lua_pushcfunction(_state, &SendBroadcastMessage);
            lua_setfield(_state, -2, "SendBroadcastMessage");
            lua_pushvalue(_state, -1);
            lua_setfield(_state, -2, "__index");
            lua_pop(_state, 1);

            lua_pushcfunction(_state, &RegisterPlayerEvent);
            lua_setglobal(_state, "RegisterPlayerEvent");

            fs::path scriptPath(sConfig.GetStringDefault("Eluna.ScriptPath", "lua_scripts"));
            std::error_code error;
            if (scriptPath.is_relative() && !fs::is_directory(scriptPath, error))
            {
                error.clear();
                fs::path parentPath = fs::path("..") / scriptPath;
                if (fs::is_directory(parentPath, error))
                    scriptPath = parentPath;
            }

            error.clear();
            if (!fs::is_directory(scriptPath, error))
            {
                sLog.outError("[Eluna]: Script path `%s` does not exist.", scriptPath.generic_string().c_str());
                sLog.outString("[Eluna]: Proof-of-concept state started with no scripts.");
                return true;
            }

            std::vector<fs::path> scripts;
            for (fs::recursive_directory_iterator it(scriptPath, error), end; it != end && !error; it.increment(error))
            {
                if (it->is_regular_file(error) && it->path().extension() == ".lua")
                    scripts.push_back(it->path());
            }
            std::sort(scripts.begin(), scripts.end());

            uint32 loaded = 0;
            for (fs::path const& script : scripts)
            {
                if (luaL_loadfile(_state, script.generic_string().c_str()) != 0)
                {
                    ReportLuaError("Could not compile", script);
                    continue;
                }

                if (lua_pcall(_state, 0, 0, 0) != 0)
                {
                    ReportLuaError("Could not execute", script);
                    continue;
                }

                ++loaded;
            }

            if (error)
                sLog.outError("[Eluna]: Could not scan script path `%s`.", scriptPath.generic_string().c_str());

            sLog.outString("[Eluna]: Loaded %u Lua script(s) from `%s`.", loaded, scriptPath.generic_string().c_str());
            sLog.outString("[Eluna]: Proof-of-concept state started.");
            return true;
        }

        void Stop()
        {
            if (!_state)
                return;

            if (_loginHandler != LUA_NOREF)
                luaL_unref(_state, LUA_REGISTRYINDEX, _loginHandler);
            _loginHandler = LUA_NOREF;
            lua_close(_state);
            _state = nullptr;
        }

        void SetLoginHandler(int reference)
        {
            if (_loginHandler != LUA_NOREF)
                luaL_unref(_state, LUA_REGISTRYINDEX, _loginHandler);
            _loginHandler = reference;
        }

        void OnLogin(Player* player)
        {
            if (!_state || _loginHandler == LUA_NOREF)
                return;

            lua_rawgeti(_state, LUA_REGISTRYINDEX, _loginHandler);
            lua_pushinteger(_state, PLAYER_EVENT_ON_LOGIN);
            PushPlayer(player);
            if (lua_pcall(_state, 2, 0, 0) != 0)
            {
                const char* error = lua_tostring(_state, -1);
                sLog.outError("[Eluna]: Login callback failed: %s", error ? error : "unknown Lua error");
                lua_pop(_state, 1);
            }
        }

    private:
        void PushPlayer(Player* player)
        {
            PlayerHandle* handle = static_cast<PlayerHandle*>(lua_newuserdata(_state, sizeof(PlayerHandle)));
            handle->player = player;
            luaL_getmetatable(_state, PLAYER_METATABLE);
            lua_setmetatable(_state, -2);
        }

        void ReportLuaError(char const* operation, fs::path const& script)
        {
            const char* error = lua_tostring(_state, -1);
            sLog.outError("[Eluna]: %s `%s`: %s", operation, script.generic_string().c_str(), error ? error : "unknown Lua error");
            lua_pop(_state, 1);
        }

        lua_State* _state = nullptr;
        int _loginHandler = LUA_NOREF;
    };

    std::unique_ptr<ElunaPoc> GlobalState;

    int RegisterPlayerEvent(lua_State* state)
    {
        ElunaPoc* engine = GetEngine(state);
        if (!engine)
            return luaL_error(state, "Eluna state is unavailable");

        int event = static_cast<int>(luaL_checkinteger(state, 1));
        luaL_checktype(state, 2, LUA_TFUNCTION);
        if (event != PLAYER_EVENT_ON_LOGIN)
            return luaL_error(state, "this proof of concept only supports player login event 3");

        lua_pushvalue(state, 2);
        engine->SetLoginHandler(luaL_ref(state, LUA_REGISTRYINDEX));
        return 0;
    }

    int SendBroadcastMessage(lua_State* state)
    {
        PlayerHandle* handle = static_cast<PlayerHandle*>(luaL_checkudata(state, 1, PLAYER_METATABLE));
        const char* message = luaL_checkstring(state, 2);
        if (handle->player && handle->player->GetSession() && message[0] != '\0')
            ChatHandler(handle->player->GetSession()).SendSysMessage(message);
        return 0;
    }

    class ElunaWorldScript : public WorldScript
    {
    public:
        ElunaWorldScript() : WorldScript("mod-eluna_world", { WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_SHUTDOWN })
        {
        }

        void OnStartup() override
        {
            GlobalState = std::make_unique<ElunaPoc>();
            if (!GlobalState->Start())
                GlobalState.reset();
        }

        void OnShutdown() override
        {
            GlobalState.reset();
        }
    };

    class ElunaPlayerScript : public PlayerScript
    {
    public:
        ElunaPlayerScript() : PlayerScript("mod-eluna_player", { PLAYERHOOK_ON_LOGIN })
        {
        }

        void OnLogin(Player* player) override
        {
            if (GlobalState)
                GlobalState->OnLogin(player);
        }
    };
}

void Addmod_elunaScripts()
{
    new ElunaWorldScript();
    new ElunaPlayerScript();
}
