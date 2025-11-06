#include <stdio.h>
#include "vip_decoy_teleport.h"

vip_decoy_teleport g_vip_decoy_teleport;

IVIPApi* g_pVIPCore;
IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayers;

IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;

int iDecoyCount[64] = {0};

PLUGIN_EXPOSE(vip_decoy_teleport, g_vip_decoy_teleport);

bool vip_decoy_teleport::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	g_SMAPI->AddListener( this, this );
	return true;
}

bool vip_decoy_teleport::Unload(char *error, size_t maxlen)
{
	delete g_pVIPCore;
	delete g_pUtils;
	return true;
}

CGameEntitySystem* GameEntitySystem()
{
    return g_pVIPCore->VIP_GetEntitySystem();
};

void VIP_OnPlayerSpawn(int iSlot, int iTeam, bool bIsVIP) {
    if (bIsVIP && g_pVIPCore->VIP_GetClientFeatureBool(iSlot, "decoy_teleport"))
    {
        CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iSlot);
        if (!pPlayerController) return;

        CCSPlayerPawnBase* pPlayerPawn = pPlayerController->m_hPlayerPawn();
        if (!pPlayerPawn) return;

        CCSPlayer_ItemServices* pItemServices = static_cast<CCSPlayer_ItemServices*>(pPlayerPawn->m_pItemServices());
        if(!pItemServices) return;

        CPlayer_WeaponServices* pWeaponServices = pPlayerPawn->m_pWeaponServices();
        if (!pWeaponServices) return;

        if (pWeaponServices->m_iAmmo()[17] == 0)
        {
            pItemServices->GiveNamedItem("weapon_decoy");
        }
    }
}

void OnDecoyFiring(const char* szName, IGameEvent* event, bool bDontBroadcast)
{
    int iSlot = event->GetInt("userid");
	
	if (!g_pVIPCore->VIP_IsClientVIP(iSlot) || !g_pVIPCore->VIP_GetClientFeatureBool(iSlot, "decoy_teleport") || g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "decoy_teleport_count") <= iDecoyCount[iSlot])
		return;
	
    CCSPlayerController* pPlayerController = CCSPlayerController::FromSlot(iSlot);
    if (!pPlayerController) return;

    CCSPlayerPawnBase* pPlayerPawn = pPlayerController->m_hPlayerPawn();
    if (!pPlayerPawn) return;

    if (pPlayerPawn->m_lifeState() != LIFE_ALIVE) return;

    float x = event->GetFloat("x");
    float y = event->GetFloat("y");
    float z = event->GetFloat("z");
    Vector vecPos(x, y, z);
    QAngle ang = pPlayerPawn->GetAbsRotation();
    Vector vel = pPlayerPawn->m_vecAbsVelocity();
	
	const char* sDTParticleIn = g_pVIPCore->VIP_GetClientFeatureString(iSlot, "decoy_teleport_particle_in");
	if (sDTParticleIn && sDTParticleIn[0] != '\0')
	{
		Vector vecAbsOrigin = pPlayerPawn->GetAbsOrigin();
		vecAbsOrigin.z += 64.0f;
		CParticleSystem* particle = (CParticleSystem*)g_pUtils->CreateEntityByName("info_particle_system", -1);
		particle->m_bStartActive(true);
		particle->m_iszEffectName(sDTParticleIn);
		g_pUtils->TeleportEntity(particle, &vecAbsOrigin, nullptr, nullptr);
		g_pUtils->DispatchSpawn(particle, nullptr);
	}

    pPlayerPawn->Teleport(&vecPos, &ang, &vel);
	
	iDecoyCount[iSlot]++;

	int left = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, "decoy_teleport_count") - iDecoyCount[iSlot];
	g_pUtils->PrintToChat(iSlot, "%s %s %d",
		g_pVIPCore->VIP_GetTranslate("Prefix"),
		g_pVIPCore->VIP_GetTranslate("DecoyCountLeft"),
		left);

	const char* sDTParticleOut = g_pVIPCore->VIP_GetClientFeatureString(iSlot, "decoy_teleport_particle_out");
	if (sDTParticleOut && sDTParticleOut[0] != '\0')
	{
		Vector vecAbsOrigin = pPlayerPawn->GetAbsOrigin();
		vecAbsOrigin.z += 64.0f;
		CParticleSystem* particle = (CParticleSystem*)g_pUtils->CreateEntityByName("info_particle_system", -1);
		particle->m_bStartActive(true);
		particle->m_iszEffectName(sDTParticleOut);
		g_pUtils->TeleportEntity(particle, &vecAbsOrigin, nullptr, nullptr);
		g_pUtils->DispatchSpawn(particle, nullptr);
	}
	
    int entityId = event->GetInt("entityid");
    CEntityInstance* pEntity = g_pGameEntitySystem->GetEntityInstance(CEntityIndex(entityId));
    if (pEntity)
    {
        g_pUtils->RemoveEntity(pEntity);
    }
}

void OnRoundStart(const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
    if (!g_pVIPCore || !g_pUtils) return;

    for (int iSlot = 0; iSlot < 64; iSlot++) {
		iDecoyCount[iSlot] = 0;
    }
}

void VIP_OnVIPLoaded()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pGameEntitySystem;
	g_pVIPCore->VIP_OnPlayerSpawn(VIP_OnPlayerSpawn);
	g_pUtils->HookEvent(g_PLID, "decoy_firing", OnDecoyFiring);
	g_pUtils->HookEvent(g_PLID, "round_start", OnRoundStart);

}

void vip_decoy_teleport::AllPluginsLoaded()
{
	int ret;
	
	g_pVIPCore = (IVIPApi*)g_SMAPI->MetaFactory(VIP_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		char error[64];
		V_strncpy(error, "Failed to lookup vip core. Aborting", 64);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	
	g_pUtils = (IUtilsApi*)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		char error[64];
		V_strncpy(error, "Failed to lookup utils api. Aborting", 64);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	
	g_pPlayers = (IPlayersApi *)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		char error[64];
		V_strncpy(error, "Failed to lookup players api. Aborting", 64);
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pVIPCore->VIP_OnVIPLoaded(VIP_OnVIPLoaded);
	g_pVIPCore->VIP_RegisterFeature("decoy_teleport", VIP_BOOL, TOGGLABLE);
	g_pVIPCore->VIP_RegisterFeature("decoy_teleport_count", VIP_INT, TOGGLABLE);
	g_pVIPCore->VIP_RegisterFeature("decoy_teleport_particle_in", VIP_STRING, HIDE);
	g_pVIPCore->VIP_RegisterFeature("decoy_teleport_particle_out", VIP_STRING, HIDE);
}

const char *vip_decoy_teleport::GetLicense()
{
	return "Public";
}

const char *vip_decoy_teleport::GetVersion()
{
	return "v1.0";
}

const char *vip_decoy_teleport::GetDate()
{
	return __DATE__;
}

const char *vip_decoy_teleport::GetLogTag()
{
	return "[VIP-DECOY-TELEPORT]";
}

const char *vip_decoy_teleport::GetAuthor()
{
	return "E!N with NovaHost";
}

const char *vip_decoy_teleport::GetDescription()
{
	return "";
}

const char *vip_decoy_teleport::GetName()
{
	return "[VIP] Decoy Teleport";
}

const char *vip_decoy_teleport::GetURL()
{
	return "https://nova-hosting.ru?ref=ein";
}
