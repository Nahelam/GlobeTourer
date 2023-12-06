#include "pch.h"
#include "GlobeTourer.h"


BAKKESMOD_PLUGIN(GlobeTourer, "Globe Tourer", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void GlobeTourer::MarkObjectInvincible(class UObject* object)
{
	if (object)
	{
		object->ObjectFlags &= ~EObjectFlags::RF_Transient;
		object->ObjectFlags &= ~EObjectFlags::RF_TagGarbageTemp;
		object->ObjectFlags |= EObjectFlags::RF_Public;
		object->ObjectFlags |= EObjectFlags::RF_Standalone;
		object->ObjectFlags |= EObjectFlags::RF_MarkAsRootSet;
		object->ObjectFlags |= EObjectFlags::RF_KeepForCooker;
	}
}

void GlobeTourer::MarkObjectForDestory(class UObject* object)
{
	if (object)
	{
		object->ObjectFlags = 0;
		object->ObjectFlags |= EObjectFlags::RF_Public;
		object->ObjectFlags |= EObjectFlags::RF_Transient;
		object->ObjectFlags |= EObjectFlags::RF_TagGarbageTemp;
	}
}

void GlobeTourer::LogSchedules(const std::string& Func, TArray<struct FScheduledTournament>& Schedules)
{
	for (FScheduledTournament& Schedule : Schedules)
	{
		LOG("({}) Tournament desc: {}", Func, Schedule.Description.ToString());
		LOG("({}) Tournament schedule id: {}", Func, Schedule.ScheduleID);
		for (const UTourSettings_TA* Tournament : Schedule.Tournaments)
		{
			LOG("({}) Tournament title: {}, region: , region name: {}", Func, Tournament->Title.ToString(), Tournament->Region.ToString(), Tournament->RegionName.ToString());
			LOG("({}) Tournament schedule id: {}", Func, Tournament->ScheduleID);
		}
	}
}

bool GlobeTourer::RegionIdValid(const std::string& regionId)
{
	bool valid = false;

	if (!regionId.empty())
	{
		for (char c : regionId)
		{
			if (!isalpha(c) || !isupper(c))
			{
				return valid;
			}
		}
		valid = true;
	}
	return valid;
}

void GlobeTourer::HookPost_SyncSchedule_0x1(ActorWrapper caller, void* params, std::string eventName)
{
	const auto p = reinterpret_cast<UAutoTour_TA_exec__AutoTour_TA__SyncSchedule_0x1_Params*>(params);

	LOG("POST: {}", eventName);

	if (!SchedulesReady)
	{
		gameWrapper->Execute([this](...)
			{
				AutoTour->SyncSchedule();
			});
	}
	else
	{
		if (AllTournamentsBuf)
		{
			p->RPC->Schedules.set_data(OriginalRPCSchedulesData);
			p->RPC->Schedules.set_size(OriginalRPCSchedulesSize);
			OriginalRPCSchedulesData = nullptr;
			OriginalRPCSchedulesSize = -1;
			delete[] AllTournamentsBuf;
			AllTournamentsBuf = nullptr;
		}
		for (const auto& Schedule : AllSchedules)
		{
			Schedule->ObjectFlags = OriginalRPCObjectFlags;
		}
		OriginalRPCObjectFlags = 0;
		AllSchedules.clear();
		SchedulesReady = false;
		TotalSchedules = 0;
		LOG("(HookPost_SyncSchedule_0x1) All good");
	}
}

void GlobeTourer::HookPre_SyncSchedule_0x1(ActorWrapper caller, void* params, std::string eventName)
{
	const auto p = reinterpret_cast<UAutoTour_TA_exec__AutoTour_TA__SyncSchedule_0x1_Params*>(params);

	LOG("PRE: {}", eventName);

	LOG("(HookPre_SyncSchedule_0x1) RPC->Region: {}", p->RPC->Region.ToString());
	LOG("(HookPre_SyncSchedule_0x1) RPC->Schedules size: {}", p->RPC->Schedules.size());

	// LogSchedules("HookPre_SyncSchedule_0x1", p->RPC->Schedules);

	if (p->RPC)
	{
		OriginalRPCObjectFlags = p->RPC->ObjectFlags;
		MarkObjectInvincible(p->RPC);
		AllSchedules.push_back(p->RPC);
		TotalSchedules += p->RPC->Schedules.size();
		LOG("(HookPre_SyncSchedule_0x1) Total saved schedules: {}", TotalSchedules);

		if (SchedulesReady)
		{
			LOG("(HookPre_SyncSchedule_0x1) All schedules retrieved, let's merge them");
			uint32_t allocSize = TotalSchedules * sizeof(FScheduledTournament);
			uint32_t numCopied = 0;
			LOG("(HookPre_SyncSchedule_0x1) Alloc size: {}", allocSize);
			AllTournamentsBuf = new FScheduledTournament[TotalSchedules];

			if (AllTournamentsBuf)
			{
				OriginalRPCSchedulesData = p->RPC->Schedules.data();
				OriginalRPCSchedulesSize = p->RPC->Schedules.size();
				for (const auto& Schedule : AllSchedules)
				{
					if (Schedule->Schedules.size() > 0)
					{
						for (auto& ScheduledTournament : Schedule->Schedules)
						{
							for (const auto& Tournament : ScheduledTournament.Tournaments)
							{
								std::string newName("[" + Schedule->Region.ToString() + "] " + Tournament->Title.ToString());
								FString__FString(&Tournament->Title, newName.c_str());
							}
						}
						size_t size = Schedule->Schedules.size() * sizeof(FScheduledTournament);
						memcpy(&AllTournamentsBuf[numCopied], Schedule->Schedules.data(), size);
						numCopied += Schedule->Schedules.size();
						LOG("(HookPre_SyncSchedule_0x1) Copied {} schedules ({} bytes) for region {}", Schedule->Schedules.size(), size, Schedule->Region.ToString());
					}
					else
					{
						LOG("(HookPre_SyncSchedule_0x1) Got no schedules for region {}", Schedule->Region.ToString());
					}
				}
				p->RPC->Schedules.set_data(AllTournamentsBuf);
				p->RPC->Schedules.set_size(TotalSchedules);
			}
			else
			{
				LOG("(HookPre_SyncSchedule_0x1) ERROR: Allocation failed");
			}
		}
	}
	else
	{
		if (SchedulesReady)
		{
			LOG("(HookPre_SyncSchedule_0x1) Got empty RPC while ready for merging, aborted");
		}
		else
		{
			LOG("(HookPre_SyncSchedule_0x1) Got empty RPC");
		}
	}
}

void GlobeTourer::HookPre_SetRegion(ActorWrapper caller, void* params, std::string eventName)
{
	const auto p = reinterpret_cast<URPC_AutoTour_GetSchedule_TA_execSetRegion_Params*>(params);

	LOG("PRE: {}", eventName);

	if (RegionsIndex >= 0)
	{
		LOG("(HookPre_SetRegion) Changing region to {}", (*AllRegions)[RegionsIndex]->Id.ToString());
		FString__OpEq(&p->InRegion, &((*AllRegions)[RegionsIndex]->Id));
		RegionsIndex++;
		if (RegionsIndex == AllRegions->size())
		{
			RegionsIndex = -1;
			SchedulesReady = true;
		}
	}
	LOG("(HookPre_SetRegion) InRegion: {}", p->InRegion.ToString());
}

void GlobeTourer::HookPre_SyncSchedule(std::string eventName)
{
	LOG("PRE: {}", eventName);

	if (RegionsIndex < 0)
	{
		if (GetRegions())
		{
			TotalSchedules = 0;
			RegionsIndex = 0;
			LOG("(HookPre_SyncSchedule) Starting schedules retrieval");
		}
		else
		{
			AutoTour->LastSyncTime = 0.0f - AutoTour->DataRefreshTime;
			LOG("(HookPre_SyncSchedule) Could not start schedules retrieval");
		}
	}
}

bool GlobeTourer::GetRegions()
{
	bool regionsOk = false;

	if (AutoTour->OnlineGame)
	{
		if (AutoTour->OnlineGame->Regions)
		{
			if (AutoTour->OnlineGame->Regions->Config)
			{
				if (AutoTour->OnlineGame->Regions->Config->Regions.size() > 0)
				{
					int32_t numEmptyIds = 0;
					const auto regions = &(AutoTour->OnlineGame->Regions->Config->Regions);
					for (int32_t i = 0; i < regions->size(); i++)
					{
						const auto& id = (*regions)[i]->Id;
						if (id.empty() || !RegionIdValid(id.ToString()))
						{
							LOG("(GetRegions) ERROR: Region {} invalid", i);
							numEmptyIds++;
						}
						else
						{
							LOG("(GetRegions) {} ({})", (*regions)[i]->Id.ToString(), i);
						}
					}
					if (numEmptyIds == 0)
					{
						AllRegions = regions;
						regionsOk = true;
					}
					LOG("(GetRegions) Got {} regions:", regions->size() - numEmptyIds);
				}
				else
				{
					LOG("(GetRegions) ERROR: AutoTour->OnlineGame->Regions->Config->Regions size is < 0");
				}
			}
			else
			{
				LOG("(GetRegions) ERROR: AutoTour->OnlineGame->Regions->Config is NULL");
			}
		}
		else
		{
			LOG("(GetRegions) ERROR: AutoTour->OnlineGame->Regions is NULL");
		}
	}
	else
	{
		LOG("(GetRegions) ERROR: AutoTour->OnlineGame is NULL");
	}

	return regionsOk;
}


void GlobeTourer::HookPre_RefreshTournamentData(ActorWrapper caller, void* params, std::string eventName)
{
	const auto actualCaller = reinterpret_cast<UAutoTour_TA*>(caller.memory_address);

	LOG("PRE: {}", eventName);

	LOG("(HookPre_RefreshTournamentData) Got AutoTour");
	AutoTour = actualCaller;
	AutoTour->LastSyncTime = 0.0f - AutoTour->DataRefreshTime;
	gameWrapper->UnhookEvent("Function TAGame.AutoTour_TA.RefreshTournamentData");
}

void GlobeTourer::onLoad()
{
	_globalCvarManager = cvarManager;

	GameBaseAddress = reinterpret_cast<uintptr_t>(GetModuleHandle(NULL));

	GObjects = reinterpret_cast<TArray<UObject*>*>(GameBaseAddress + GObjObjectsOffset);
	GNames = reinterpret_cast<TArray<FNameEntry*>*>(GameBaseAddress + NamesOffset);

	FString__FString = reinterpret_cast<FString__FString_t>(GameBaseAddress + FString__FStringOffset);
	FString__OpEq = reinterpret_cast<FString__OpEq_t>(GameBaseAddress + FString__OpEqOffset);

	AutoTour = nullptr;
	AllRegions = nullptr;
	AllTournamentsBuf = nullptr;
	OriginalRPCSchedulesData = nullptr;
	OriginalRPCSchedulesSize = -1;
	OriginalRPCObjectFlags = 0;
	TotalSchedules = -1;
	RegionsIndex = -1;
	SchedulesReady = false;
	AllSchedules.clear();

	gameWrapper->HookEvent("Function TAGame.AutoTour_TA.SyncSchedule", std::bind_front(&GlobeTourer::HookPre_SyncSchedule, this));
	gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.AutoTour_TA.RefreshTournamentData", std::bind_front(&GlobeTourer::HookPre_RefreshTournamentData, this));
	gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.AutoTour_TA.__AutoTour_TA__SyncSchedule_0x1", std::bind_front(&GlobeTourer::HookPre_SyncSchedule_0x1, this));
	gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.RPC_AutoTour_GetSchedule_TA.SetRegion", std::bind_front(&GlobeTourer::HookPre_SetRegion, this));
	gameWrapper->HookEventWithCallerPost<ActorWrapper>("Function TAGame.AutoTour_TA.__AutoTour_TA__SyncSchedule_0x1", std::bind_front(&GlobeTourer::HookPost_SyncSchedule_0x1, this));
}

void GlobeTourer::onUnload()
{
	gameWrapper->UnhookEvent("Function TAGame.AutoTour_TA.RefreshTournamentData");
	gameWrapper->UnhookEvent("Function TAGame.AutoTour_TA.SyncSchedule");
	gameWrapper->UnhookEvent("Function TAGame.AutoTour_TA.__AutoTour_TA__SyncSchedule_0x1");
	gameWrapper->UnhookEvent("Function TAGame.RPC_AutoTour_GetSchedule_TA.SetRegion");
	gameWrapper->UnhookEventPost("Function TAGame.AutoTour_TA.__AutoTour_TA__SyncSchedule_0x1");
}
