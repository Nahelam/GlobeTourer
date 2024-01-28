#include "pch.h"
#include "GlobeTourer.h"

BAKKESMOD_PLUGIN(GlobeTourer, "Globe Tourer", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

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

void GlobeTourer::HookPost_SyncSchedule_0x1(ActorWrapper caller, void* params, std::string eventName)
{
    const auto p = reinterpret_cast<UAutoTour_TA_exec__AutoTour_TA__SyncSchedule_0x1_Params*>(params);

    LOG("POST: {}", eventName);

    if (!SchedulesReady)
    {
        gameWrapper->Execute([this](...)
            {
                OnlineGameTournaments->AutoTour->SyncSchedule();
            });
    }
    else
    {
        if (AllTournamentsBuf)
        {
            p->RPC->Schedules.set_data(OriginalRPCSchedulesData);
            p->RPC->Schedules.set_size(OriginalRPCSchedulesSize);
        }
        CleanUp();
        LOG("(HookPost_SyncSchedule_0x1) All good");
    }
}

void GlobeTourer::HookPre_SyncSchedule_0x1(ActorWrapper caller, void* params, std::string eventName)
{
    const auto p = reinterpret_cast<UAutoTour_TA_exec__AutoTour_TA__SyncSchedule_0x1_Params*>(params);

    LOG("PRE: {}", eventName);

    //LogSchedules("HookPre_SyncSchedule_0x1", p->RPC->Schedules);

    if (p->RPC)
    {
        LOG("(HookPre_SyncSchedule_0x1) RPC->Region: {}", p->RPC->Region.ToString());
        LOG("(HookPre_SyncSchedule_0x1) RPC->Schedules size: {}", p->RPC->Schedules.size());
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

    LOG("(HookPre_SetRegion) Changing region to {}", Regions[RegionsIndex]);
    FString__FString(&p->InRegion, Regions[RegionsIndex]);
    RegionsIndex++;
    if (RegionsIndex == ARRAY_LENGTH(Regions))
    {
        SchedulesReady = true;
    }
    LOG("(HookPre_SetRegion) InRegion: {}", p->InRegion.ToString());
}

void GlobeTourer::CleanUp()
{
    UnsetHooks();

    if (AllTournamentsBuf)
    {
        delete[] AllTournamentsBuf;
    }

    InitVars();
}

void GlobeTourer::InitVars()
{
    OnlineGameTournaments = nullptr;
    AllTournamentsBuf = nullptr;
    OriginalRPCSchedulesData = nullptr;
    OriginalRPCSchedulesSize = -1;
    TotalSchedules = -1;
    RegionsIndex = -1;
    InProgress = false;
    SchedulesReady = false;
    AllSchedules.clear();
}

void GlobeTourer::SetHooks()
{
    gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.RPC_AutoTour_GetSchedule_TA.SetRegion", std::bind_front(&GlobeTourer::HookPre_SetRegion, this));
    gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.AutoTour_TA.__AutoTour_TA__SyncSchedule_0x1", std::bind_front(&GlobeTourer::HookPre_SyncSchedule_0x1, this));
    gameWrapper->HookEventWithCallerPost<ActorWrapper>("Function TAGame.AutoTour_TA.__AutoTour_TA__SyncSchedule_0x1", std::bind_front(&GlobeTourer::HookPost_SyncSchedule_0x1, this));
}

void GlobeTourer::UnsetHooks()
{
    gameWrapper->UnhookEventPost("Function TAGame.AutoTour_TA.__AutoTour_TA__SyncSchedule_0x1");
    gameWrapper->UnhookEvent("Function TAGame.AutoTour_TA.__AutoTour_TA__SyncSchedule_0x1");
    gameWrapper->UnhookEvent("Function TAGame.RPC_AutoTour_GetSchedule_TA.SetRegion");
    gameWrapper->UnhookEvent("Function TAGame.OnlineGameTournaments_TA.GetIsRegistered");
}

void GlobeTourer::PrepareForRefresh()
{
    TotalSchedules = 0;
    RegionsIndex = 0;
}

void GlobeTourer::DoRefresh()
{
    gameWrapper->Execute([this](...)
        {
            OnlineGameTournaments->AutoTour->RefreshTournamentData(true);
        });
}

void GlobeTourer::HookPre_GetIsRegistered(ActorWrapper caller, void* params, std::string eventName)
{
    const auto actualCaller = reinterpret_cast<UOnlineGameTournaments_TA*>(caller.memory_address);

    LOG("PRE: {}", eventName);

    if (!InProgress)
    {
        InProgress = true;
        OnlineGameTournaments = actualCaller;
        LOG("(HookPre_GetIsRegistered) Got OnlineGameTournaments");
        PrepareForRefresh();
        SetHooks();
        DoRefresh();
    }
    else
    {
        LOG("(HookPre_GetIsRegistered) Retrieval already in progress");
    }
}

void GlobeTourer::OnGetAllSchedules(std::vector<std::string> params)
{
    InitVars();
    gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.OnlineGameTournaments_TA.GetIsRegistered", std::bind_front(&GlobeTourer::HookPre_GetIsRegistered, this));
}

void GlobeTourer::onLoad()
{
    _globalCvarManager = cvarManager;

    GameBaseAddress = reinterpret_cast<uintptr_t>(GetModuleHandle(NULL));

    GObjects = reinterpret_cast<TArray<UObject*>*>(GameBaseAddress + GObjObjectsOffset);
    GNames = reinterpret_cast<TArray<FNameEntry*>*>(GameBaseAddress + NamesOffset);

    FString__FString = reinterpret_cast<FString__FString_t>(GameBaseAddress + FString__FStringOffset);

    cvarManager->registerNotifier("get_all_schedules", std::bind_front(&GlobeTourer::OnGetAllSchedules, this), "Get all tournament schedules", PERMISSION_ALL);
}
