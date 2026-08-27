#include "ElunaModule.h"

#include "ElunaConfig.h"
#include "ElunaLoader.h"
#include "ElunaMgr.h"
#include "LuaEngine.h"
#include "ScriptObjects.h"
#include "Log.h"

namespace
{
    ElunaInfoKey const GlobalStateKey = ElunaInfoKey::MakeGlobalKey(0);
    Eluna* GlobalState = nullptr;

    class ElunaWorldScript : public WorldScript
    {
    public:
        ElunaWorldScript() : WorldScript("mod-eluna_world", { WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_SHUTDOWN })
        {
        }

        void OnStartup() override
        {
            sElunaConfig->Initialize();
            if (!sElunaConfig->IsElunaEnabled())
            {
                sLog.outString("[Eluna]: Disabled by configuration.");
                return;
            }

            sElunaLoader->LoadScripts();
            sElunaMgr->Create(nullptr, ElunaInfo(GlobalStateKey));
            GlobalState = sElunaMgr->Get(GlobalStateKey);
            sLog.outString("[Eluna]: Proof-of-concept state started.");
        }

        void OnShutdown() override
        {
            if (!GlobalState)
                return;

            sElunaMgr->Destroy(GlobalStateKey);
            GlobalState = nullptr;
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
