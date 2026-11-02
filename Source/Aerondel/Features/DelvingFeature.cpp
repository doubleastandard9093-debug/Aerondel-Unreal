#include "Features/DelvingFeature.h"
#include "Features/AerondelGates.h"
#include "Features/RelicryData.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/HealthComponent.h"
#include "Combat/AerondelMob.h"
#include "Rules/AerondelRules.h"
#include "Dom/JsonObject.h"

namespace
{
	const TCHAR* Roman[] = { TEXT(""), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V") };
	const TCHAR* KeyNames[] = { TEXT("Crimson"), TEXT("Azure"), TEXT("Gold"), TEXT("Jade") };
	const int32 GearTokens[] = { 2600, 2400, 2400, 1100, 2200, 1500, 1500, 1500, 800 }; // order of RelicryData::DelveGear
	struct FRecipe { const TCHAR* Item; const TCHAR* Skill; int32 Ore, Logs, Qty; };
	const FRecipe Recipes[] = { { TEXT("Delve blade"), TEXT("Smithing"), 3, 1, 1 }, { TEXT("Delve plate"), TEXT("Smithing"), 5, 0, 1 }, { TEXT("Delve greaves"), TEXT("Smithing"), 4, 0, 1 },
		{ TEXT("Delve buckler"), TEXT("Crafting"), 1, 2, 1 }, { TEXT("Delve shortbow"), TEXT("Fletching"), 0, 3, 1 }, { TEXT("Delve arrows"), TEXT("Fletching"), 1, 1, 40 } };
	FString GatherItem(const FString& Skill) { return Skill == TEXT("Mining") ? TEXT("Delve ore") : Skill == TEXT("Woodcutting") ? TEXT("Delve logs") : Skill == TEXT("Fishing") ? TEXT("Raw cavefish") : FString(); }
}

void UDelvingFeature::SaveState(FJsonObject& Out) const
{
	Out.SetNumberField(TEXT("tokens"), Tokens); Out.SetNumberField(TEXT("floors"), Floors);
	Out.SetNumberField(TEXT("best"), Best); Out.SetNumberField(TEXT("deepest"), Deepest);
}

void UDelvingFeature::LoadState(const FJsonObject& In)
{
	Tokens = FMath::Max<int32>(0, JInt(In, TEXT("tokens"))); Floors = FMath::Max<int32>(0, JInt(In, TEXT("floors")));
	Best = FMath::Max<int32>(0, JInt(In, TEXT("best"))); Deepest = FMath::Max<int32>(0, JInt(In, TEXT("deepest")));
}

FString UDelvingFeature::Enter(FName Size, int32 Difficulty)
{
	if (bInRun) return TEXT("You are already below.");
	const int32 Side = Size == FName(TEXT("large")) ? 4 : Size == FName(TEXT("medium")) ? 3 : 2;
	Rooms = Side == 4 ? 9 : Side == 3 ? 7 : 5;
	Diff = FMath::Clamp(Difficulty, 1, Unlocked()); Level = FMath::Min(110, 8 + Diff * 9); Tier = FMath::Min(5, (Diff + 1) / 2);
	bInRun = true; bBossDead = false; RunTicks = 0; Reached = 0; Reward.Reset(); GatherSkill.Reset();
	// Main path: Rooms-1 doors; the last (into the guardian's room) stays open. Side-room keys lock later doors, the rest are gates.
	Doors.Reset(); FloorKeys.Reset();
	const int32 NDoors = Rooms - 1;
	for (int32 i = 0; i < NDoors; ++i) { FDelveDoor D; D.Kind = i == NDoors - 1 ? TEXT("open") : TEXT("gate"); D.bOpen = i == NDoors - 1; Doors.Add(D); }
	for (int32 k = 0; k < Side && k < 4; ++k)
	{
		const int32 i = 1 + k * 2; if (i >= NDoors - 1) break;
		Doors[i].Kind = TEXT("key"); Doors[i].Key = KeyNames[k]; Doors[i].Name = FString(KeyNames[k]) + TEXT(" door"); FloorKeys.Add(KeyNames[k]);
	}
	const float Ease = UAerondelRules::Perk(GetPS(), TEXT("delving"), TEXT("gateEase"));
	TArray<FString> Gs = AerondelGates::Draw(TMap<FString, int32>(), NDoors); int32 g = 0;
	for (int32 i = 0; i < NDoors; ++i) if (Doors[i].Kind == TEXT("gate"))
	{
		FDelveDoor& D = Doors[i]; D.Skill = Gs.IsValidIndex(g) ? Gs[g++] : FString(TEXT("Agility")); D.Need = AerondelGates::GateLevel(Level, i, Ease);
		const RelicryData::FGate* G = AerondelGates::Find(D.Skill); D.Name = G ? G->Name : D.Skill;
	}
	GiveItem(TEXT("Cavefish"), 4);
	SpawnFoe(false, Hub() + FVector2D(-3.f, 10.f)); // first room
	return FString::Printf(TEXT("Delving floor: %s, difficulty %d. Gather, craft and fight your way to the guardian."), *Size.ToString(), Diff);
}

void UDelvingFeature::SpawnFoe(bool bBoss, FVector2D Pos)
{
	const int32 L = bBoss ? Level + 10 : Level;
	const int32 BL = bBoss ? 60 : 30, BHp = bBoss ? 1100 : 300, BMax = bBoss ? 40 : 26, BAtt = bBoss ? 42 : 26, BDef = bBoss ? 40 : 24;
	const float K = FMath::Max(0.2f, L / float(BL));
	const TCHAR* N = bBoss ? TEXT("Floor guardian") : TEXT("Delve fiend");
	AAerondelMob* M = SpawnTunedMob(N, Pos, N, FMath::RoundToInt(BHp * K), FMath::Max(3, FMath::RoundToInt(BMax * FMath::Sqrt(K))),
		FMath::Max(1, FMath::RoundToInt(BAtt * K)), FMath::Max(1, FMath::RoundToInt(BDef * K)), L, bBoss ? 1.7f : 1.f, bBoss);
	if (!M) return;
	Live.Add(M); if (bBoss) Boss = M;
}

void UDelvingFeature::OnGameTick()
{
	++TickN;
	if (!bInRun) return;
	++RunTicks;
	for (int32 i = Live.Num() - 1; i >= 0; --i)
	{
		AAerondelMob* M = Live[i].Get();
		if (!M) { Live.RemoveAt(i); continue; }
		if (!M->IsDead() || M == Boss.Get()) continue;
		GiveXp(TEXT("Delving"), 6.f + Diff * 2.f); Live.RemoveAt(i);
	}
	if (!bBossDead && Boss.IsValid() && Boss->IsDead())
	{
		bBossDead = true;
		const int32 Xp = FMath::RoundToInt(40.f * Rooms * (1.f + Diff * 0.6f) + Level * 8.f);
		const int32 Tk = FMath::RoundToInt(Rooms * Diff * 12 * (1.f + UAerondelRules::Perk(GetPS(), TEXT("delving"), TEXT("tokenBonus"))));
		GiveXp(TEXT("Delving"), Xp); Tokens += Tk; ++Floors; Deepest = FMath::Max(Deepest, Diff);
		const int32 Secs = FMath::RoundToInt(RunTicks * 0.6f); if (!Best || Secs < Best) Best = Secs;
		Say(FString::Printf(TEXT("Floor cleared! +%d Delving XP and %d tokens (%d total). Take the stair up when ready."), Xp, Tk, Tokens));
		if (FMath::FRand() < 0.015f + Diff * 0.004f) { Reward = RelicryData::DelveGear[FMath::RandHelper((int32)UE_ARRAY_COUNT(RelicryData::DelveGear))]; Say(FString::Printf(TEXT("The guardian drops the %s! It waits for you above."), *Reward)); }
	}
	if (GatherSkill.IsEmpty() || TickN < GatherNext) return;
	GatherNext = TickN + 3;
	if (FMath::FRand() < 0.55f + SkillLevel(GatherSkill) * 0.004f)
	{
		GiveItem(GatherItem(GatherSkill), FMath::FRand() < UAerondelRules::Perk(GetPS(), TEXT("delving"), TEXT("double")) ? 2 : 1);
		GiveXp(GatherSkill, 10.f + Level * 0.6f); GiveXp(TEXT("Delving"), 4.f + Diff);
	}
}

FString UDelvingFeature::Gather(const FString& Skill)
{
	if (!bInRun || GatherItem(Skill).IsEmpty()) return FString();
	const int32 Need = FMath::Max(1, (Tier - 1) * 15);
	if (SkillLevel(Skill) < Need) return FString::Printf(TEXT("You need %s %d."), *Skill, Need);
	GatherSkill = Skill; GatherNext = TickN + 2;
	return FString::Printf(TEXT("You start gathering %s."), *GatherItem(Skill).ToLower());
}

TArray<FString> UDelvingFeature::GetRecipes() const
{
	TArray<FString> O;
	for (int32 t = 1; t <= Tier; ++t) for (const FRecipe& R : Recipes) O.Add(FString::Printf(TEXT("%s %s (%s %d; %d ore, %d logs)"), R.Item, Roman[t], R.Skill, (t - 1) * 18 + 1, R.Ore, R.Logs));
	return O;
}

FString UDelvingFeature::Forge(int32 Index)
{
	const int32 N = (int32)UE_ARRAY_COUNT(Recipes); if (!bInRun || Index < 0 || Index >= Tier * N) return FString();
	const int32 t = Index / N + 1; const FRecipe& R = Recipes[Index % N]; const int32 L = (t - 1) * 18 + 1;
	if (SkillLevel(R.Skill) < L) return FString::Printf(TEXT("You need %s %d."), R.Skill, L);
	if (CountItem(TEXT("Delve ore")) < R.Ore || CountItem(TEXT("Delve logs")) < R.Logs) return TEXT("You lack the materials.");
	if (R.Ore) TakeItem(TEXT("Delve ore"), R.Ore);
	if (R.Logs) TakeItem(TEXT("Delve logs"), R.Logs);
	const FString Item = FString::Printf(TEXT("%s %s"), R.Item, Roman[t]);
	GiveItem(Item, R.Qty); GiveXp(R.Skill, 20.f + L * 3.f); GiveXp(TEXT("Delving"), 5.f + Diff);
	return FString::Printf(TEXT("You make %s."), *Item);
}

FString UDelvingFeature::Cook()
{
	const int32 N = CountItem(TEXT("Raw cavefish")); if (!bInRun || N <= 0) return TEXT("You have no raw cavefish.");
	int32 Ok = 0;
	for (int32 i = 0; i < N; ++i) { TakeItem(TEXT("Raw cavefish"), 1); if (FMath::FRand() < 0.6f + SkillLevel(TEXT("Cooking")) * 0.005f) { GiveItem(TEXT("Cavefish"), 1); ++Ok; } else GiveItem(TEXT("Burnt cavefish"), 1); }
	GiveXp(TEXT("Cooking"), Ok * (20.f + Level));
	return FString::Printf(TEXT("You cook %d of %d cavefish."), Ok, N);
}

FString UDelvingFeature::TakeKey(int32 Index)
{
	if (!bInRun || !FloorKeys.IsValidIndex(Index) || FloorKeys[Index].IsEmpty()) return FString();
	const FString K = FloorKeys[Index] + TEXT(" key"); FloorKeys[Index].Reset();
	SpawnFoe(false, Hub() + FVector2D(6.f, 12.f + Index * 6.f)); // side rooms are guarded
	GiveItem(K, 1); return FString::Printf(TEXT("You take the %s."), *K.ToLower());
}

void UDelvingFeature::Opened()
{
	AAerondelPlayerState* PS = GetPS();
	const float H = UAerondelRules::Perk(PS, TEXT("delving"), TEXT("floorHeal"));
	if (H > 0.f && PS && PS->Health && !PS->Health->IsDead()) PS->Health->Heal(FMath::RoundToInt(PS->Health->GetMax() * H));
	GiveXp(TEXT("Delving"), 15.f + Level);
}

FString UDelvingFeature::OpenDoor(int32 Index)
{
	if (!bInRun || !Doors.IsValidIndex(Index)) return FString();
	for (int32 i = 0; i < Index; ++i) if (!Doors[i].bOpen) return TEXT("An earlier door still blocks the way.");
	FDelveDoor& D = Doors[Index];
	FString Msg;
	if (!D.bOpen)
	{
		if (D.Kind == TEXT("key"))
		{
			if (!TakeItem(D.Key + TEXT(" key"), 1)) return FString::Printf(TEXT("Locked. It needs the %s key."), *D.Key.ToLower());
			Msg = FString::Printf(TEXT("The %s door swings open."), *D.Key.ToLower());
		}
		else
		{
			if (SkillLevel(D.Skill) < D.Need) return FString::Printf(TEXT("You need %s %d."), *D.Skill, D.Need);
			const RelicryData::FGate* G = AerondelGates::Find(D.Skill);
			if (G && FString(G->Kind) == TEXT("hp"))
			{
				AAerondelPlayerState* PS = GetPS(); if (!PS || !PS->Health) return FString();
				const int32 C = FMath::RoundToInt(PS->Health->GetMax() * 0.1f); if (PS->Health->GetCurrent() <= C + 1) return TEXT("Too weak.");
				PS->Health->ApplyDamage(C, nullptr);
			}
			GiveXp(G && FString(G->Kind) == TEXT("fight") ? FString(TEXT("Attack")) : D.Skill, 20.f + D.Need * 5.f);
			Msg = G ? FString(G->Done) : FString(TEXT("The way opens."));
		}
		D.bOpen = true; Opened();
	}
	// Walking through a door reaches the next room: foes there (1-3), and the guardian behind the last door.
	if (Index + 1 > Reached)
	{
		Reached = Index + 1;
		const FVector2D R = Hub() + FVector2D(0.f, 10.f + Reached * 8.f);
		if (Index == Doors.Num() - 1) SpawnFoe(true, R);
		else for (int32 j = 0; j < 1 + (Reached % 2) + (Diff > 5 ? 1 : 0); ++j) SpawnFoe(false, R + FVector2D(-3.f + j * 3.f, 2.f));
	}
	return Msg.IsEmpty() ? TEXT("You go through.") : Msg;
}

TArray<FString> UDelvingFeature::GetShop() const
{
	TArray<FString> O; for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(RelicryData::DelveGear); ++i) O.Add(FString::Printf(TEXT("%s - %d tokens"), RelicryData::DelveGear[i], i < (int32)UE_ARRAY_COUNT(GearTokens) ? GearTokens[i] : 9999));
	return O;
}

FString UDelvingFeature::Buy(int32 Index)
{
	if (Index < 0 || Index >= (int32)UE_ARRAY_COUNT(RelicryData::DelveGear) || Index >= (int32)UE_ARRAY_COUNT(GearTokens)) return FString();
	if (Tokens < GearTokens[Index]) return FString::Printf(TEXT("You need %d tokens; you have %d."), GearTokens[Index], Tokens);
	Tokens -= GearTokens[Index]; GiveItem(RelicryData::DelveGear[Index], 1);
	return FString::Printf(TEXT("The %s is yours."), RelicryData::DelveGear[Index]);
}

void UDelvingFeature::Finish(bool bWin)
{
	if (!bInRun) return;
	bInRun = false; GatherSkill.Reset();
	for (const TWeakObjectPtr<AAerondelMob>& M : Live) if (M.IsValid()) M->Destroy();
	if (Boss.IsValid()) Boss->Destroy();
	Live.Reset(); Boss.Reset(); Doors.Reset(); FloorKeys.Reset();
	// Floor-bound items crumble when you leave.
	TArray<FString> Bound = { TEXT("Delve ore"), TEXT("Delve logs"), TEXT("Raw cavefish"), TEXT("Cavefish"), TEXT("Burnt cavefish") };
	for (const TCHAR* K : KeyNames) Bound.Add(FString(K) + TEXT(" key"));
	for (int32 t = 1; t <= 5; ++t) for (const FRecipe& R : Recipes) Bound.Add(FString::Printf(TEXT("%s %s"), R.Item, Roman[t]));
	for (const FString& B : Bound) { const int32 N = CountItem(B); if (N > 0) TakeItem(B, N); }
	if (!Reward.IsEmpty()) { GiveItem(Reward, 1); Reward.Reset(); }
	TeleportPlayer(Hub() + FVector2D(0.f, 4.f));
	if (!bWin) Say(TEXT("You leave the floor. Floor-bound items crumble away."));
}
