#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

#define REDACTED 0
constexpr uintptr_t GObjObjectsOffset = REDACTED;
constexpr uintptr_t NamesOffset = REDACTED;
constexpr uintptr_t FString__FStringOffset = REDACTED;
constexpr uintptr_t FString__OpEqOffset = REDACTED;

class GlobeTourer: public BakkesMod::Plugin::BakkesModPlugin
{
	uintptr_t GameBaseAddress;

	using FString__OpEq_t = void(__cdecl*)(FString* _this, FString* Other);
	FString__OpEq_t FString__OpEq;

	using FString__FString_t = void(__cdecl*)(FString* _this, const char* In);
	FString__FString_t FString__FString;

	UAutoTour_TA* AutoTour;
	TArray<class URegion_X*>* AllRegions;
	TArray<class URPC_AutoTour_GetSchedule_TA*> AllSchedules;
	FScheduledTournament* AllTournamentsBuf;
	FScheduledTournament* OriginalRPCSchedulesData;
	uint64_t OriginalRPCObjectFlags;
	int32_t OriginalRPCSchedulesSize;
	int32_t TotalSchedules;
	int32_t RegionsIndex;
	bool SchedulesReady;

	void onLoad() override;
	void onUnload() override;

	void MarkObjectInvincible(class UObject* object);
	void MarkObjectForDestory(class UObject* object);

	bool GetRegions();
	bool RegionIdValid(const std::string& regionId);
	void HookPre_RefreshTournamentData(ActorWrapper caller, void* params, std::string eventName);
	void HookPre_SyncSchedule(std::string eventName);
	void HookPre_SetRegion(ActorWrapper caller, void* params, std::string eventName);
	void HookPre_SyncSchedule_0x1(ActorWrapper caller, void* params, std::string eventName);
	void HookPost_SyncSchedule_0x1(ActorWrapper caller, void* params, std::string eventName);
	void LogSchedules(const std::string& Func, TArray<struct FScheduledTournament>& Schedules);
};
