#include "ElunaModule.h"

#include "ElunaRuntime.h"
#include "ScriptObjects.h"

#include <memory>

namespace
{
    std::unique_ptr<Eluna::Runtime> GlobalState;

    class ElunaWorldScript : public WorldScript
    {
    public:
        ElunaWorldScript()
            : WorldScript("mod-eluna_world", { WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_UPDATE, WORLDHOOK_ON_SHUTDOWN })
        {
        }

        void OnStartup() override
        {
            // World startup runs on MainThread in this core. Lua is prepared
            // lazily and owned by WorldThread so login callbacks never share a
            // lua_State across threads.
            if (!GlobalState)
                GlobalState = std::make_unique<Eluna::Runtime>();
        }

        void OnUpdate(uint32 /*diff*/) override
        {
            if (GlobalState)
                GlobalState->Start();
        }

        void OnShutdown() override
        {
            if (GlobalState)
            {
                GlobalState->Stop();
                GlobalState.reset();
            }
        }
    };

    class ElunaPlayerScript : public PlayerScript
    {
    public:
        ElunaPlayerScript()
            : PlayerScript("mod-eluna_player", { PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_BEFORE_LOGOUT })
        {
        }

        void OnLogin(Player* player) override
        {
            if (GlobalState)
                GlobalState->OnLogin(player);
        }

        void OnBeforeLogout(Player* player) override
        {
            if (GlobalState)
                GlobalState->OnBeforeLogout(player);
        }
    };
}

void Addmod_elunaScripts()
{
    new ElunaWorldScript();
    new ElunaPlayerScript();
}
