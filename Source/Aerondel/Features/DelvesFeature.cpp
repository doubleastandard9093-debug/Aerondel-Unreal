#include "Features/DelvesFeature.h"
#include "Core/AerondelEvents.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/HealthComponent.h"
#include "Data/MobDef.h"

namespace DelveData
{
	struct FG { const TCHAR* Mob; int32 N; };
	struct FD { const TCHAR* Id; const TCHAR* Name; int32 Level; FG Rooms[3][2]; int32 Coins; int32 Xp; };
	static const FD All[] = {
		{ TEXT("sewers"), TEXT("The Undercroft Sewers"), 20, { { { TEXT("Zombie"), 4 }, { nullptr, 0 } }, { { TEXT("Giant Spider"), 3 }, { TEXT("Zombie"), 2 } }, { { TEXT("Skeleton"), 5 }, { TEXT("Bog Giant"), 1 } } }, 600, 2500 },
		{ TEXT("keep"), TEXT("The Drowned Keep"), 35, { { { TEXT("Drowned Deckhand"), 5 }, { nullptr, 0 } }, { { TEXT("Drowned Gunner"), 3 }, { TEXT("Drowned Deckhand"), 3 } }, { { TEXT("Drowned Gunner"), 4 }, { TEXT("Ogre"), 2 } } }, 1400, 6000 } };
	static const FD* Find(FName Id) { for (const FD& D : All) if (Id == FName(D.Id)) return &D; return nullptr; }
}

void UDelvesFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UAerondelEvents* Ev = Collection.InitializeDependency<UAerondelEvents>();
	Super::Initialize(Collection);
	if (Ev) Ev->OnMobKilled.AddDynamic(this, &UDelvesFeature::HandleMobKilled);
}

bool UDelvesFeature::SetRole(FName NewRole)
{
	if (NewRole != FName(TEXT("tank")) && NewRole != FName(TEXT("healer")) && NewRole != FName(TEXT("dps"))) return false;
	Role = NewRole; return true;
}

FString UDelvesFeature::Enter(FName DelveId)
{
	const DelveData::FD* D = DelveData::Find(DelveId);
	if (!D) return TEXT("Unknown delve.");
	if (CombatLevel() < D->Level) return FString::Printf(TEXT("%s needs combat level %d."), D->Name, D->Level);
	Current = DelveId; Room = 0; Ticks = 0; StartRoom();
	return FString::Printf(TEXT("You enter %s as the %s."), D->Name, *Role.ToString());
}

void UDelvesFeature::StartRoom()
{
	Left = 0; for (const FDelveSpawn& S : GetRoom()) Left += S.Count;
}

TArray<FDelveSpawn> UDelvesFeature::GetRoom() const
{
	TArray<FDelveSpawn> Out; const DelveData::FD* D = DelveData::Find(Current);
	if (!D || Room < 0 || Room > 2) return Out;
	int32 Total = 0;
	for (const DelveData::FG& G : D->Rooms[Room]) if (G.Mob) { FDelveSpawn S; S.Mob = Id(G.Mob); S.Count = FMath::Min(G.N, RoomCap - Total); Total += S.Count; if (S.Count > 0) Out.Add(S); }
	return Out;
}

void UDelvesFeature::HandleMobKilled(UMobDef* Mob)
{
	if (Current.IsNone() || !Mob || Left <= 0) return;
	bool bInRoom = false; for (const FDelveSpawn& S : GetRoom()) if (S.Mob == Mob->Id) bInRoom = true;
	if (!bInRoom || --Left > 0) return;
	const DelveData::FD* D = DelveData::Find(Current);
	if (++Room < 3) { StartRoom(); Say(FString::Printf(TEXT("Room cleared. Onward to room %d."), Room + 1)); return; }
	const FString K = Current.ToString();
	Clears.FindOrAdd(K) += 1;
	const int32 Secs = FMath::RoundToInt(Ticks * 0.6f);
	if (!Best.Contains(K) || Secs < Best[K]) Best.Add(K, Secs);
	if (D) { GiveCoins(D->Coins); GiveXp(TEXT("Hitpoints"), D->Xp / 2.f); GiveXp(Role == FName(TEXT("healer")) ? TEXT("Prayer") : Role == FName(TEXT("tank")) ? TEXT("Defence") : TEXT("Strength"), D->Xp / 2.f); Say(FString::Printf(TEXT("%s cleared in %ds! +%d coins."), D->Name, Secs, D->Coins)); }
	Leave();
}

float UDelvesFeature::DamageTakenMult(const UMobDef* Source, float HpFrac) const
{
	if (Current.IsNone()) return 1.f;
	return Role == FName(TEXT("tank")) ? 0.75f : Role == FName(TEXT("healer")) ? 0.7f : 0.8f;
}

float UDelvesFeature::DamageDealtBonus(const UMobDef* Target) const
{
	if (Current.IsNone()) return 0.f;
	return Role == FName(TEXT("tank")) ? 0.05f : Role == FName(TEXT("healer")) ? 0.f : 0.15f;
}

void UDelvesFeature::OnGameTick()
{
	if (Current.IsNone()) return;
	++Ticks;
	AAerondelPlayerState* PS = GetPS(); if (!PS || !PS->Health || PS->Health->IsDead()) return;
	RegenAcc += (Role == FName(TEXT("healer")) ? 4.f : 2.f) / 6.f;
	if (RegenAcc >= 1.f) { const int32 N = FMath::FloorToInt(RegenAcc); RegenAcc -= N; PS->Health->Heal(N); }
}

void UDelvesFeature::SaveState(FJsonObject& Out) const
{
	PutIntMap(Out, TEXT("clears"), Clears); PutIntMap(Out, TEXT("best"), Best); Out.SetStringField(TEXT("role"), Role.ToString());
}

void UDelvesFeature::LoadState(const FJsonObject& In)
{
	for (const TPair<FString, int32>& P : JIntMap(In, TEXT("clears"))) if (DelveData::Find(FName(*P.Key))) Clears.Add(P.Key, FMath::Max(0, P.Value));
	for (const TPair<FString, int32>& P : JIntMap(In, TEXT("best"))) if (DelveData::Find(FName(*P.Key)) && P.Value > 0) Best.Add(P.Key, P.Value);
	if (!SetRole(FName(*JStr(In, TEXT("role"))))) Role = TEXT("dps");
}
