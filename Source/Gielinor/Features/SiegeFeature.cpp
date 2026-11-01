#include "Features/SiegeFeature.h"
#include "Features/ArenaFeature.h"
#include "Features/CompanionsFeature.h"
#include "Features/DelvesFeature.h"
#include "Core/GielinorEvents.h"
#include "Core/GielinorPlayerState.h"
#include "Combat/HealthComponent.h"
#include "Combat/GielinorMob.h"
#include "Data/MobDef.h"

namespace SiegeData
{
	static const FVector2D Center(-1000, -640); static constexpr float R = 16.f;
	static const FVector2D DefaultReturn(-50, -84);
	static const TCHAR* Sellswords[] = { TEXT("Garrow the Hired"), TEXT("Lisk Ironpay"), TEXT("Old Tamberly"), TEXT("Wenna Shieldwright"), TEXT("Dace the Lamplighter") };
	struct FB { const TCHAR* Id; const TCHAR* Name; const TCHAR* Base; float Scale; int32 Hp, MaxHit, Att, Def; bool bFire, bStack, bAdds; };
	static const FB Bosses[] = {
		{ TEXT("gorrak"), TEXT("Gorrak Gatebreaker"), TEXT("Ogre"), 1.8f, 2600, 60, 60, 50, true, false, false },
		{ TEXT("matron"), TEXT("The Cinder Matron"), TEXT("Cinder Fiend"), 1.6f, 2800, 64, 64, 54, true, false, true },
		{ TEXT("ironjaw"), TEXT("Ironjaw"), TEXT("Ice Giant"), 1.8f, 3000, 66, 66, 58, true, true, false },
		{ TEXT("warchief"), TEXT("Warchief Vaskarr"), TEXT("Ironveil Knight"), 1.7f, 3600, 72, 72, 62, true, true, true } };
	static constexpr int32 N = UE_ARRAY_COUNT(Bosses);
	// Mechanics: every 8 ticks, 3-tick warning. Fire r3 = 30% max HP. Stack r3.2 = 40% split across the party inside.
	static constexpr int32 Every = 8, Warn = 3, Adds = 2; static constexpr float FireR = 3.f, FireDmg = 0.3f, StackR = 3.2f, StackDmg = 0.4f;
	static const TCHAR* AddType = TEXT("Ashen raider");
	static const TCHAR* Loot[N][2] = { { TEXT("Cobalt full helm"), TEXT("Cobalt shield") }, { TEXT("Cobalt platelegs"), nullptr }, { TEXT("Cobalt platebody"), nullptr }, { TEXT("Emberite ore"), TEXT("Cobalt bar") } };
	static bool Valid(const FString& Id) { for (const FB& B : Bosses) if (Id == B.Id) return true; return false; }
}

void USiegeFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UGielinorEvents* Ev = Collection.InitializeDependency<UGielinorEvents>();
	Super::Initialize(Collection);
	if (Ev) Ev->OnMobKilled.AddDynamic(this, &USiegeFeature::HandleMobKilled);
}

void USiegeFeature::Lock() const
{
	const int64 W = UArenaFeature::WeekNo(NowMs());
	if (W != Week) { Week = W; Downed.Reset(); }
}

int32 USiegeFeature::DownedThisWeek() const { Lock(); return Downed.Num(); }

bool USiegeFeature::Enter()
{
	Lock();
	if (bRunning) return false;
	if (const UDelvesFeature* D = Feature<UDelvesFeature>()) if (D->IsInside()) { Say(TEXT("Finish your delve first.")); return false; }
	if (Downed.Num() >= SiegeData::N) { Say(TEXT("The Warband is broken for this week. Come back after the weekly reset.")); return false; }
	Party.Reset();
	if (const UCompanionsFeature* C = Feature<UCompanionsFeature>()) if (const FCompanionDef* Def = UCompanionsFeature::Def(C->GetActive())) Party.Add(Def->Name);
	const int32 Off = (int32)(Week % UE_ARRAY_COUNT(SiegeData::Sellswords));
	for (int32 i = 0; i < 3 && Party.Num() < FriendCap - 1; ++i) Party.Add(SiegeData::Sellswords[(Off + i) % UE_ARRAY_COUNT(SiegeData::Sellswords)]);
	if (!PlayerWebPos(Return)) Return = SiegeData::DefaultReturn;
	bRunning = true; BossIdx = Downed.Num(); Tick = 0; MechAt = SiegeData::Every; MechN = 0; bPending = false;
	TeleportPlayer(SiegeData::Center + FVector2D(0, SiegeData::R - 3));
	Say(FString::Printf(TEXT("You march on the fort with %s."), *FString::Join(Party, TEXT(", "))));
	SpawnBoss();
	return true;
}

int32 USiegeFeature::Alive() const
{
	int32 N = 0; for (const TWeakObjectPtr<AGielinorMob>& M : Hostiles) if (M.IsValid() && !M->IsDead()) ++N; return N;
}

void USiegeFeature::SpawnBoss()
{
	const SiegeData::FB& B = SiegeData::Bosses[BossIdx];
	bAddsDone = false;
	AGielinorMob* M = SpawnTunedMob(B.Base, SiegeData::Center + FVector2D(0, -5), B.Name, B.Hp, B.MaxHit, B.Att, B.Def, 120, B.Scale, true);
	Boss = M; if (M) Hostiles.Add(M);
	Say(FString::Printf(TEXT("Boss %d/%d: %s."), BossIdx + 1, SiegeData::N, B.Name));
}

void USiegeFeature::StartMech(FName Kind)
{
	using namespace SiegeData;
	if (Kind == FName(TEXT("adds")))
	{
		int32 Spawned = 0;
		for (int32 i = 0; i < Adds; ++i)
			if (Alive() < HostileCap) if (AGielinorMob* M = SpawnTunedMob(AddType, Center + FVector2D(-6 + i * 12, -2), AddType, 420, 38, 32, 28, 40)) { Hostiles.Add(M); ++Spawned; }
		Say(FString::Printf(TEXT("%d Ashen raiders rush in!"), Spawned));
		return;
	}
	FVector2D P; if (!PlayerWebPos(P)) return;
	bPending = true; PendingKind = Kind; PendingTick = Tick + Warn;
	if (Kind == FName(TEXT("fire"))) { PendingAt = P; Say(TEXT("Fire spreads under you! Move out of the red ring.")); }
	else { PendingAt = Center + FVector2D(0, 4); Say(TEXT("A crushing blow is coming! Stack with your party in the blue ring to share it.")); }
}

void USiegeFeature::ResolveMech()
{
	using namespace SiegeData;
	bPending = false;
	FVector2D P; const AGielinorPlayerState* PS = GetPS();
	if (!PlayerWebPos(P) || !PS || !PS->Health) return;
	const float D = FVector2D::Distance(P, PendingAt), Max = PS->Health->GetMax();
	if (PendingKind == FName(TEXT("fire")) && D < FireR) HurtPlayer(Max * FireDmg, TEXT("The fire"));
	else if (PendingKind == FName(TEXT("stack"))) HurtPlayer(Max * StackDmg / (D < StackR ? Party.Num() + 1 : 1), TEXT("The blow"));
}

bool USiegeFeature::GetPendingMech(FSiegeMech& Out) const
{
	if (!bRunning || !bPending) return false;
	Out.Kind = PendingKind; Out.Location = WebToWorld(PendingAt.X, PendingAt.Y);
	Out.RadiusCm = (PendingKind == FName(TEXT("fire")) ? SiegeData::FireR : SiegeData::StackR) * 100.f;
	Out.TicksLeft = FMath::Max(0, PendingTick - Tick);
	return true;
}

void USiegeFeature::OnGameTick()
{
	using namespace SiegeData;
	if (!bRunning) return;
	++Tick;
	if (bPending && Tick >= PendingTick) ResolveMech();
	AGielinorMob* B = Boss.Get();
	if (B && !B->IsDead())
	{
		// The party chips at the boss: 0.4% of its max HP per member every 3 ticks.
		if (Tick % 3 == 0 && B->Health) B->Health->ApplyDamage(FMath::Max(1, FMath::RoundToInt(B->GetMaxHp() * 0.004f * Party.Num())), nullptr);
		if (!bPending && Tick >= MechAt)
		{
			const FB& Def = Bosses[BossIdx];
			TArray<FName> Ms;
			if (Def.bFire) Ms.Add(TEXT("fire")); if (Def.bStack) Ms.Add(TEXT("stack")); if (Def.bAdds && !bAddsDone) Ms.Add(TEXT("adds"));
			if (Ms.Num()) { const FName K = Ms[MechN++ % Ms.Num()]; if (K == FName(TEXT("adds"))) bAddsDone = true; StartMech(K); }
			MechAt = Tick + Every;
		}
	}
}

void USiegeFeature::HandleMobKilled(UMobDef* Mob)
{
	using namespace SiegeData;
	if (!bRunning) return;
	AGielinorMob* B = Boss.Get();
	if (B && B->Definition == Mob)
	{
		Lock();
		const FB& Def = Bosses[BossIdx];
		if (!Downed.Contains(Def.Id))
		{
			Downed.Add(Def.Id);
			const TCHAR* Opts[2] = { Loot[BossIdx][0], Loot[BossIdx][1] };
			const TCHAR* It = Opts[1] && FMath::RandBool() ? Opts[1] : Opts[0];
			GiveCoins(BossCoins); GiveItem(It, 1);
			Say(FString::Printf(TEXT("%s is defeated! +%d coins, %s."), Def.Name, BossCoins, It));
		}
		Boss = nullptr;
	}
	if (Boss.IsValid() || Alive() > 0) return;
	if (BossIdx < N - 1) { ++BossIdx; MechAt = Tick + Every; MechN = 0; SpawnBoss(); return; }
	++Clears;
	End(TEXT("The Ashen Warband is broken! The siege is won."));
}

void USiegeFeature::End(const FString& Msg)
{
	bRunning = false; bPending = false;
	for (const TWeakObjectPtr<AGielinorMob>& M : Hostiles) if (M.IsValid()) M->Destroy();
	Hostiles.Reset(); Boss = nullptr;
	Say(Msg);
	if (AGielinorPlayerState* PS = GetPS()) if (PS->Health && !PS->Health->IsDead()) TeleportPlayer(Return);
}

FString USiegeFeature::Greeting() const
{
	return FString::Printf(TEXT("Hesk, warcaptain. The Ashen Warband holds the old fort: four war-leaders. Bring your companions; I'll hire three sellswords. Down this week: %d/%d. Sieges won: %d."), DownedThisWeek(), SiegeData::N, Clears);
}

FString USiegeFeature::HowItWorks()
{
	return TEXT("Get out of red fire. Stack in the blue ring to share the big blow. Kill the raiders when they come. Bosses you down stay down until the weekly reset.");
}

void USiegeFeature::SaveState(FJsonObject& Out) const
{
	Lock();
	Out.SetNumberField(TEXT("week"), (double)Week); PutStrArr(Out, TEXT("downed"), Downed); Out.SetNumberField(TEXT("clears"), Clears);
}

void USiegeFeature::LoadState(const FJsonObject& In)
{
	Week = JInt(In, TEXT("week")); Downed.Reset();
	for (const FString& S : JStrArr(In, TEXT("downed"))) if (SiegeData::Valid(S)) Downed.AddUnique(S);
	Clears = FMath::Max<int32>(0, (int32)JInt(In, TEXT("clears")));
	Lock();
}
