#pragma once

#ifndef _ELUNA_UTIL_H
#define _ELUNA_UTIL_H

#include "Common.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"
#include "Log.h"
#include "Database/QueryResult.h"

#include <memory>
#include <vector>

#define EXP_CLASSIC 0
#define EXP_TBC 1
#define EXP_WOTLK 2
#define EXP_CATA 3

typedef std::shared_ptr<QueryNamedResult> ElunaQuery;
typedef std::vector<uint8> BytecodeBuffer;

#define ASSERT MANGOS_ASSERT
#define ELUNA_LOG_INFO(...) sLog.outString(__VA_ARGS__)
#define ELUNA_LOG_ERROR(...) sLog.outError(__VA_ARGS__)
#define ELUNA_LOG_DEBUG(...) sLog.outDebug(__VA_ARGS__)
#define GET_GUID GetObjectGuid
#define GetGameObjectTemplate GetGameObjectInfo
#define GetItemTemplate GetItemPrototype
#define GetTemplate GetProto
#define MAKE_NEW_GUID(l, e, h) ObjectGuid(h, e, l)
#define GUID_ENPART(guid) ObjectGuid(guid).GetEntry()
#define GUID_LOPART(guid) ObjectGuid(guid).GetCounter()
#define GUID_HIPART(guid) ObjectGuid(guid).GetHigh()

namespace ElunaUtil
{
    uint32 GetCurrTime();
    uint32 GetTimeDiff(uint32 oldMSTime);
}

#endif

#ifndef _ELUNA_INCLUDES_H
#define _ELUNA_INCLUDES_H

#include "AccountMgr.h"
#include "Chat.h"
#include "Config/Config.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "DBCEnums.h"
#include "DBCStores.h"
#include "GameEventMgr.h"
#include "GameObject.h"
#include "Guild.h"
#include "InstanceData.h"
#include "Item.h"
#include "Map.h"
#include "MapManager.h"
#include "Object.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellEntry.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Weather.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldPacket.h"

#define CORE_NAME "Tortoise-WoW"
#define CORE_VERSION "tortoise-wow"
#define DEFAULT_LOCALE LOCALE_enUS
#define eWorld (&sWorld)
#define eMapMgr (&sMapMgr)
#define eConfigMgr (&sConfig)
#define eGuildMgr (&sGuildMgr)
#define eObjectMgr (&sObjectMgr)
#define eAccountMgr (&sAccountMgr)
#define eGameEventMgr (&sGameEventMgr)
#define eObjectAccessor() sObjectAccessor.
#define SERVER_MSG_STRING SERVER_MSG_CUSTOM
#define TOTAL_LOCALES MAX_LOCALE
#define TARGETICONCOUNT TARGET_ICON_COUNT
#define MAX_TALENT_SPECS MAX_TALENT_SPEC_COUNT
#define TEAM_NEUTRAL TEAM_INDEX_NEUTRAL

#endif

#ifndef _ELUNA_CREATURE_AI_H
#define _ELUNA_CREATURE_AI_H

struct ElunaCreatureAI : CreatureAI
{
    explicit ElunaCreatureAI(Creature* creature) : CreatureAI(creature) {}
};

#endif

#ifndef _ELUNA_INSTANCE_DATA_H
#define _ELUNA_INSTANCE_DATA_H

class ElunaInstanceAI : public InstanceData
{
public:
    explicit ElunaInstanceAI(Map* map) : InstanceData(map) {}
};

#endif
