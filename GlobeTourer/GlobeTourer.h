#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

#define REDACTED 0
constexpr uintptr_t GObjObjectsOffset = REDACTED;
constexpr uintptr_t NamesOffset = REDACTED;
constexpr uintptr_t FString__FStringOffset = REDACTED;

constexpr const char* Regions[] = {
	"USE",
	"EU",
	"USW",
	"ASC",
	"ASM",
	"JPN",
	"ME",
	"OCE",
	"SAF",
	"SAM",
	"IND"
};

class GlobeTourer: public BakkesMod::Plugin::BakkesModPlugin
{
	uintptr_t GameBaseAddress;

	using FString__FString_t = void(__cdecl*)(FString* _this, const char* In);
	FString__FString_t FString__FString;

	UOnlineGameTournaments_TA* OnlineGameTournaments;
	TArray<class URPC_AutoTour_GetSchedule_TA*> AllSchedules;
	FScheduledTournament* AllTournamentsBuf;
	FScheduledTournament* OriginalRPCSchedulesData;
	uint64_t OriginalRPCObjectFlags;
	int32_t OriginalRPCSchedulesSize;
	int32_t TotalSchedules;
	int32_t RegionsIndex;
	bool InProgress;
	bool SchedulesReady;

	void onLoad() override;

	void HookPre_SetRegion(ActorWrapper caller, void* params, std::string eventName);
	void HookPre_SyncSchedule_0x1(ActorWrapper caller, void* params, std::string eventName);
	void HookPost_SyncSchedule_0x1(ActorWrapper caller, void* params, std::string eventName);
	void HookPre_GetIsRegistered(ActorWrapper caller, void* params, std::string eventName);
	void LogSchedules(const std::string& Func, TArray<struct FScheduledTournament>& Schedules);

	void OnGetAllSchedules(std::vector<std::string> params);
	void InitVars();
	void CleanUp();
	void SetHooks();
	void UnsetHooks();
	void PrepareForRefresh();
	void DoRefresh();
};
