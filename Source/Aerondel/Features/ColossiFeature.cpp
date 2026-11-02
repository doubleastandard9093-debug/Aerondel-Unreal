#include "Features/ColossiFeature.h"
#include "Features/ArenaFeature.h"
#include "Features/StandingFeature.h"
#include "Features/StillwatchFeature.h"
#include "Features/FlowFeature.h"
#include "Core/AerondelEvents.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/HealthComponent.h"
#include "Combat/AerondelMob.h"
#include "Data/MobDef.h"
#include "AIController.h"

namespace ColossiData
{
	struct FC { const TCHAR* Id; const TCHAR* Name; const TCHAR* Title; const TCHAR* Base; float Scale; int32 Hp, MaxHit, Att, Def, Level; FVector2D Route[5]; };
	static const FC All[3] = {
		{ TEXT("stonehorn"), TEXT("Stonehorn"), TEXT("Stonehorn the Walking Hill"), TEXT("Ogre"), 4.f, 4200, 70, 70, 60, 140, { FVector2D(96, -100), FVector2D(120, -92), FVector2D(146, -100), FVector2D(150, -80), FVector2D(110, -80) } },
		{ TEXT("frostmaw"), TEXT("Frostmaw"), TEXT("Frostmaw of the Cold Scar"), TEXT("Ice Giant"), 3.5f, 4600, 76, 74, 66, 150, { FVector2D(160, -110), FVector2D(172, -130), FVector2D(166, -150), FVector2D(150, -140), FVector2D(152, -118) } },
		{ TEXT("ashwyrm"), TEXT("Ashwyrm"), TEXT("Ashwyrm, Old Cinderback"), TEXT("Green Dragon"), 3.f, 5200, 84, 80, 70, 165, { FVector2D(96, -150), FVector2D(118, -164), FVector2D(140, -160), FVector2D(128, -144), FVector2D(104, -138) } } };
	static constexpr float HornShare = 0.12f, LegsShare = 0.15f, BackBonus = 0.2f, WakeR = 110.f, ParkR = 160.f;
	static constexpr int32 ChargeEvery = 14, ChargeWarn = 2, ChestCoins = 4000, ChestTokens = 25, ChestRep = 60;
	static constexpr float ChargeRange = 7.f, ChargeDmg = 0.22f;
	static constexpr int64 RespawnMs = 10LL * 60 * 1000;
	static FVector2D Web(const FVector& V) { return FVector2D(V.X / 100.0, V.Y / 100.0); }
}

void UColossiFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UAerondelEvents* Ev = Collection.InitializeDependency<UAerondelEvents>();
	Super::Initialize(Collection);
	if (Ev) Ev->OnMobKilled.AddDynamic(this, &UColossiFeature::HandleMobKilled);
}

void UColossiFeature::Deinitialize()
{
	for (int32 i = 0; i < 3; ++i) if (Live[i].Mob.IsValid()) Live[i].Mob->Destroy();
	Super::Deinitialize();
}

bool UColossiFeature::IsBehind(FVector2D MobPos, FVector2D MobForward, FVector2D Attacker)
{
	const FVector2D D = Attacker - MobPos; const double Len = D.Size();
	return Len > KINDA_SMALL_NUMBER && FVector2D::DotProduct(MobForward.GetSafeNormal(), D / Len) < -0.5;
}

bool UColossiFeature::Behind(const AAerondelMob* M) const
{
	FVector2D P; if (!M || !PlayerWebPos(P)) return false;
	const FVector F = M->GetActorForwardVector();
	return IsBehind(ColossiData::Web(M->GetActorLocation()), FVector2D(F.X, F.Y), P);
}

float UColossiFeature::DamageDealtBonus(const UMobDef* Target) const
{
	for (const FLive& L : Live) if (L.Mob.IsValid() && L.Mob->Definition == Target) return Behind(L.Mob.Get()) ? ColossiData::BackBonus : 0.f;
	return 0.f;
}

void UColossiFeature::Wake(int32 Idx)
{
	const ColossiData::FC& C = ColossiData::All[Idx];
	AAerondelMob* M = SpawnTunedMob(C.Base, C.Route[0], C.Name, C.Hp, C.MaxHit, C.Att, C.Def, C.Level, C.Scale, true);
	if (!M) return;
	M->WanderRadius = 2500.f; // roams its stretch of the route (see NEXT_STEPS: full waypoint walking)
	FLive& L = Live[Idx]; L = FLive(); L.Mob = M; L.LastHp = C.Hp;
	L.Horn = FMath::RoundToInt(C.Hp * ColossiData::HornShare); L.Legs = FMath::RoundToInt(C.Hp * ColossiData::LegsShare);
	Say(FString::Printf(TEXT("The ground shakes. %s stirs."), C.Title));
}

void UColossiFeature::Park(int32 Idx)
{
	if (Live[Idx].Mob.IsValid()) Live[Idx].Mob->Destroy();
	Live[Idx] = FLive();
}

void UColossiFeature::DamagePart(int32 Idx, bool bHorn, int32 D)
{
	FLive& L = Live[Idx]; int32& Part = bHorn ? L.Horn : L.Legs;
	if (Part <= 0 || D <= 0) return;
	Part -= D; if (Part > 0) return;
	++Parts;
	const ColossiData::FC& C = ColossiData::All[Idx];
	if (bHorn) { L.ChargeAt = 0; Say(FString::Printf(TEXT("You break %s's horn! It can no longer charge."), C.Name)); }
	else { if (AAerondelMob* M = L.Mob.Get()) if (M->Definition) M->Definition->AttackSeconds += 1.2f; Say(FString::Printf(TEXT("%s's legs buckle! It attacks more slowly."), C.Name)); }
}

void UColossiFeature::OnGameTick()
{
	using namespace ColossiData;
	++Tick;
	FVector2D P; const bool bHasP = PlayerWebPos(P);
	// Part damage: read HP lost since the last tick (all sources the player causes).
	for (int32 i = 0; i < 3; ++i)
	{
		FLive& L = Live[i]; AAerondelMob* M = L.Mob.Get();
		if (!M || M->IsDead()) continue;
		const int32 Hp = M->GetHp(), D = L.LastHp - Hp; L.LastHp = Hp;
		if (D <= 0) continue;
		const UStillwatchFeature* SW = Feature<UStillwatchFeature>();
		if (SW && SW->IsActive() && SW->GetSpot() == EWeakSpot::Legs) DamagePart(i, false, D * 2);
		else if (SW && SW->IsActive() && SW->GetSpot() == EWeakSpot::Head) DamagePart(i, true, D * 2);
		else DamagePart(i, !Behind(M), D);
	}
	if (Tick % 5 || !bHasP) return;
	const int64 Now = NowMs();
	for (int32 i = 0; i < 3; ++i) // every colossus has its own area, so "one awake per area" = one per entry
	{
		const float D = FVector2D::Distance(P, All[i].Route[0]); FLive& L = Live[i];
		if (L.Mob.IsValid() && !L.Mob->IsDead() && D > ParkR) Park(i);
		else if (!L.Mob.IsValid() && D < WakeR && Now >= NextAt[i]) Wake(i);
	}
	for (int32 i = 0; i < 3; ++i)
	{
		FLive& L = Live[i]; AAerondelMob* M = L.Mob.Get();
		if (!M || M->IsDead()) continue;
		const bool bFighting = M->GetState() == EMobState::Chase || M->GetState() == EMobState::Attack;
		if (!bFighting)
		{
			if (Tick % 25 == 0) { L.Leg = (L.Leg + 1) % 5; if (AAIController* C = Cast<AAIController>(M->GetController())) C->MoveToLocation(WebToWorld(All[i].Route[L.Leg].X, All[i].Route[L.Leg].Y, M->GetActorLocation().Z / 100.0)); }
			continue;
		}
		if (L.Horn <= 0) continue;
		if (!L.ChargeAt && Tick % ChargeEvery == 0) { L.ChargeAt = Tick + ChargeWarn; Say(FString::Printf(TEXT("%s lowers its horn to charge! Get behind it or roll away."), All[i].Name)); }
		else if (L.ChargeAt && Tick >= L.ChargeAt)
		{
			L.ChargeAt = 0;
			const AAerondelPlayerState* PS = GetPS();
			if (PS && PS->Health && FVector2D::Distance(P, Web(M->GetActorLocation())) < ChargeRange && !Behind(M))
				HurtPlayer(PS->Health->GetMax() * ChargeDmg, FString::Printf(TEXT("%s's charge"), All[i].Name));
		}
	}
}

void UColossiFeature::HandleMobKilled(UMobDef* Mob)
{
	using namespace ColossiData;
	for (int32 i = 0; i < 3; ++i)
	{
		FLive& L = Live[i];
		if (!L.Mob.IsValid() || L.Mob->Definition != Mob) continue;
		Kills.FindOrAdd(All[i].Id) += 1; NextAt[i] = NowMs() + RespawnMs;
		GiveCoins(ChestCoins);
		if (UArenaFeature* A = Feature<UArenaFeature>()) A->AddTokens(ChestTokens);
		if (UStandingFeature* St = Feature<UStandingFeature>()) St->AddRep(TEXT("watch"), ChestRep);
		Say(FString::Printf(TEXT("%s falls! Its chest holds %d coins and %d Wilds tokens (+%d Watch standing)."), All[i].Name, ChestCoins, ChestTokens, ChestRep));
		AAerondelMob* M = L.Mob.Get(); L = FLive();
		if (M) M->SetLifeSpan(5.f);
		return;
	}
}

TArray<FColossusInfo> UColossiFeature::List() const
{
	TArray<FColossusInfo> Out;
	for (int32 i = 0; i < 3; ++i)
	{
		FColossusInfo I; const ColossiData::FC& C = ColossiData::All[i]; const FLive& L = Live[i];
		I.Id = C.Id; I.Title = C.Title; I.Kills = Kills.FindRef(C.Id); I.bAwake = L.Mob.IsValid() && !L.Mob->IsDead();
		I.bHornBroken = I.bAwake && L.Horn <= 0; I.bLegsBroken = I.bAwake && L.Legs <= 0; I.bCharging = L.ChargeAt != 0;
		Out.Add(I);
	}
	return Out;
}

void UColossiFeature::SaveState(FJsonObject& Out) const
{
	TMap<FString, int32> K; for (const ColossiData::FC& C : ColossiData::All) K.Add(C.Id, Kills.FindRef(C.Id));
	PutIntMap(Out, TEXT("kills"), K); Out.SetNumberField(TEXT("parts"), Parts);
}

void UColossiFeature::LoadState(const FJsonObject& In)
{
	const TMap<FString, int32> K = JIntMap(In, TEXT("kills"));
	for (const ColossiData::FC& C : ColossiData::All) Kills.Add(C.Id, FMath::Max(0, K.FindRef(C.Id)));
	Parts = FMath::Max<int32>(0, (int32)JInt(In, TEXT("parts")));
}
