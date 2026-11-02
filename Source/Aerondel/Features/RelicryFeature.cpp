#include "Features/RelicryFeature.h"
#include "Features/RelicryData.h"
#include "Features/AerondelGates.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/HealthComponent.h"
#include "Combat/AerondelMob.h"
#include "Rules/AerondelRules.h"
#include "Dom/JsonObject.h"

namespace
{
	constexpr int32 NumSites = (int32)UE_ARRAY_COUNT(RelicryData::Sites);
	const RelicryData::FSite* SiteAt(int32 i) { return i >= 0 && i < NumSites ? &RelicryData::Sites[i] : nullptr; }
	struct FMobBase { const TCHAR* Name; int32 Level, Hp, MaxHit, Att, Def; float Scale; };
	const FMobBase Sentinel = { TEXT("Relic sentinel"), 40, 400, 30, 30, 30, 1.05f };
	const FMobBase WardenB = { TEXT("Vault warden"), 50, 500, 34, 34, 34, 1.15f };
	const FMobBase Colossus = { TEXT("Relic colossus"), 80, 1500, 50, 50, 50, 1.8f };
	const TCHAR* Mattocks[] = { TEXT("Bronze mattock"), TEXT("Iron mattock"), TEXT("Steel mattock"), TEXT("Bluesteel mattock"), TEXT("Verdite mattock"), TEXT("Cobalt mattock"), TEXT("Emberite mattock") };
}

FString URelicryFeature::DamagedName(const FString& Art) { return Art.IsEmpty() ? Art : TEXT("Damaged ") + Art.Left(1).ToLower() + Art.Mid(1); }

void URelicryFeature::SaveState(FJsonObject& Out) const
{
	PutIntMap(Out, TEXT("coll"), Coll); PutIntMap(Out, TEXT("seen"), Seen);
	Out.SetNumberField(TEXT("runs"), Runs); Out.SetNumberField(TEXT("best"), Best);
}

void URelicryFeature::LoadState(const FJsonObject& In)
{
	ResetState();
	for (const auto& P : JIntMap(In, TEXT("coll"))) Coll.Add(P.Key, FMath::Max(0, P.Value));
	for (const auto& P : JIntMap(In, TEXT("seen"))) if (AerondelGates::Find(P.Key)) Seen.Add(P.Key, FMath::Max(0, P.Value));
	Runs = FMath::Max<int32>(0, JInt(In, TEXT("runs"))); Best = FMath::Max<int32>(0, JInt(In, TEXT("best")));
}

void URelicryFeature::ResetState()
{
	Coll.Reset(); Seen.Reset(); Runs = 0; Best = 0; DigSite = -1; bInRun = false; bBossDead = false; bChest = false; Gates.Reset(); Live.Reset();
	for (const RelicryData::FSite& S : RelicryData::Sites) Coll.Add(S.Name, 0);
}

TArray<FString> URelicryFeature::GetSites() const { TArray<FString> O; for (const RelicryData::FSite& S : RelicryData::Sites) O.Add(S.Name); return O; }

int32 URelicryFeature::NearestSite() const
{
	FVector2D P; if (!PlayerWebPos(P)) return -1;
	for (int32 i = 0; i < NumSites; ++i) if (FVector2D::Distance(P, FVector2D(RelicryData::Sites[i].X, RelicryData::Sites[i].Z)) < 12.f) return i;
	return -1;
}

int32 URelicryFeature::MattockTier(FString& OutName) const
{
	for (int32 t = (int32)UE_ARRAY_COUNT(Mattocks) - 1; t >= 0; --t)
		if (CountItem(Mattocks[t]) > 0) { OutName = Mattocks[t]; return t; }
	return -1;
}

FString URelicryFeature::StartDig(int32 Site)
{
	const RelicryData::FSite* S = SiteAt(Site); if (!S) return TEXT("No dig site here.");
	if (Lvl() < S->Level) return FString::Printf(TEXT("You need Relicry level %d to excavate the %s. The Drowned Chapel in Mirelow Fen takes beginners."), S->Level, S->Name);
	FString Tool;
	if (MattockTier(Tool) < 0) { GiveItem(TEXT("Bronze mattock"), 1); Tool = TEXT("Bronze mattock"); Say(TEXT("A site warden lends you a Bronze mattock. Better mattocks are smithed at an anvil.")); }
	DigSite = Site; DigNext = TickN + 2;
	return FString::Printf(TEXT("You start excavating with your %s. Move away to stop."), *Tool.ToLower());
}

void URelicryFeature::OnGameTick()
{
	++TickN;
	if (bInRun)
	{
		++RunTicks;
		if (Warden.IsValid() && Warden->IsDead())
		{
			Warden.Reset();
			for (FRelicGate& G : Gates) if (!G.bOpen) { G.bOpen = true; GiveXp(TEXT("Attack"), 20 + G.Need * 6); GiveXp(TEXT("Relicry"), 10 + G.Need * 2); Say(TEXT("The warden falls and the gate grinds open.")); break; }
		}
		if (!bBossDead && Boss.IsValid() && Boss->IsDead()) { bBossDead = true; GiveXp(TEXT("Relicry"), 150 + RunLevel * 12); Say(TEXT("The Relic colossus crumbles! The reliquary chest unseals.")); }
		return;
	}
	const RelicryData::FSite* S = SiteAt(DigSite); if (!S) return;
	FVector2D P;
	if (!PlayerWebPos(P) || FVector2D::Distance(P, FVector2D(S->X, S->Z)) > 12.f || (GetPS() && GetPS()->Health && GetPS()->Health->IsDead())) { DigSite = -1; return; }
	if (TickN < DigNext) return;
	const AAerondelPlayerState* PS = GetPS();
	auto Perk = [PS](const TCHAR* K) { return UAerondelRules::Perk(PS, TEXT("relicry"), K); };
	DigNext = TickN + (FMath::FRand() < Perk(TEXT("fastGather")) ? 2 : 3);
	FString Tool; const int32 Tier = FMath::Max(0, MattockTier(Tool));
	const float Chance = FMath::Min(0.9f, 0.3f + (Lvl() - S->Level) * 0.01f + Tier * 0.05f);
	if (FMath::FRand() >= Chance) return;
	GiveXp(TEXT("Relicry"), 8.f + Lvl() * 0.9f);
	const int32 Dbl = FMath::FRand() < Perk(TEXT("double")) ? 2 : 1; const float R = FMath::FRand();
	if (FMath::FRand() < Perk(TEXT("rareFind"))) { GiveItem(TEXT("Ancient fragment"), 1); Say(TEXT("Lost Hoards: an Ancient fragment!")); }
	if (R < 0.16f)
	{
		TArray<const RelicryData::FArt*> Ok; for (const RelicryData::FArt& A : S->Arts) if (Lvl() >= A.Level) Ok.Add(&A);
		if (Ok.Num()) { const FString Art = Ok[FMath::RandHelper(Ok.Num())]->Name; GiveItem(DamagedName(Art), 1); Say(FString::Printf(TEXT("You unearth a %s!"), *DamagedName(Art).ToLower())); }
	}
	else if (R < 0.5f) GiveItem(S->Mat, Dbl);
	else if (R < 0.72f) GiveItem(TEXT("Relic shard"), Dbl);
	else if (R < 0.74f) { GiveItem(TEXT("Ancient fragment"), 1); Say(TEXT("An Ancient fragment glints in the soil!")); }
}

FString URelicryFeature::Restore(int32 Site, int32 ArtIdx)
{
	const RelicryData::FSite* S = SiteAt(Site); if (!S || ArtIdx < 0 || ArtIdx > 2) return TEXT("Nothing to restore.");
	const RelicryData::FArt& A = S->Arts[ArtIdx]; const bool bFrag = A.Level >= 30;
	if (Lvl() < A.Level) return FString::Printf(TEXT("You need Relicry %d to restore that."), A.Level);
	if (CountItem(DamagedName(A.Name)) < 1) return TEXT("You have no damaged piece of that.");
	if (CountItem(S->Mat) < 2 || CountItem(TEXT("Relic shard")) < 1 || (bFrag && CountItem(TEXT("Ancient fragment")) < 1))
		return FString::Printf(TEXT("You need 2 %s, 1 Relic shard%s."), S->Mat, bFrag ? TEXT(" and 1 Ancient fragment") : TEXT(""));
	const bool bSave = FMath::FRand() < UAerondelRules::Perk(GetPS(), TEXT("relicry"), TEXT("saveMat"));
	TakeItem(DamagedName(A.Name), 1);
	if (!bSave) { TakeItem(S->Mat, 2); TakeItem(TEXT("Relic shard"), 1); if (bFrag) TakeItem(TEXT("Ancient fragment"), 1); }
	GiveItem(A.Name, 1); GiveXp(TEXT("Relicry"), 40.f + A.Level * 4.f);
	return FString::Printf(TEXT("You restore the %s.%s"), A.Name, bSave ? TEXT(" (Curator: materials saved.)") : TEXT(""));
}

FString URelicryFeature::TurnIn(int32 Site)
{
	const RelicryData::FSite* S = SiteAt(Site); if (!S) return FString();
	for (const RelicryData::FArt& A : S->Arts) if (CountItem(A.Name) < 1) return TEXT("A collection needs one of each restored artifact.");
	for (const RelicryData::FArt& A : S->Arts) TakeItem(A.Name, 1);
	int32& N = Coll.FindOrAdd(S->Name); const bool bFirst = N == 0; ++N;
	GiveXp(TEXT("Relicry"), 300.f + S->Level * 30.f); GiveCoins(1000 + S->Level * 60); GiveItem(TEXT("Ancient fragment"), 2);
	if (bFirst) { GiveItem(S->Reward, 1); return FString::Printf(TEXT("Collection complete! The curators award you the %s."), S->Reward); }
	return FString::Printf(TEXT("The %s collection is complete again (%d)."), S->Name, N);
}

FString URelicryFeature::Reforge(const FString& Gear)
{
	for (const RelicryData::FReforge& R : RelicryData::Reforges)
	{
		if (Gear != R.Gear) continue;
		const RelicryData::FSite* S = nullptr; for (const RelicryData::FSite& X : RelicryData::Sites) if (FCString::Strcmp(X.Name, R.Site) == 0) S = &X;
		if (!S) return FString();
		if (Lvl() < R.Level) return FString::Printf(TEXT("You need Relicry %d to reforge the %s."), R.Level, R.Gear);
		const TCHAR* Art = nullptr; for (const RelicryData::FArt& A : S->Arts) if (!Art && CountItem(A.Name) > 0) Art = A.Name;
		if (CountItem(TEXT("Ancient fragment")) < R.Frags || CountItem(S->Mat) < R.Mat || !Art)
			return FString::Printf(TEXT("You need %d Ancient fragments, %d %s and a restored artifact from this site."), R.Frags, R.Mat, S->Mat);
		TakeItem(TEXT("Ancient fragment"), R.Frags); TakeItem(S->Mat, R.Mat); TakeItem(Art, 1);
		GiveItem(R.Gear, 1); GiveXp(TEXT("Relicry"), 2000.f + R.Level * 40.f);
		return FString::Printf(TEXT("The %s gleams as if new."), R.Gear);
	}
	return TEXT("That cannot be reforged.");
}

int32 URelicryFeature::DungeonLevel(int32 Site) const
{
	const RelicryData::FSite* S = SiteAt(Site); if (!S) return 1;
	return FMath::Max(S->Level, FMath::Min(99, FMath::RoundToInt((S->Level + Lvl()) / 2.f)));
}

void URelicryFeature::SpawnScaled(const FString& Type, FVector2D Pos, bool bBoss)
{
	const FMobBase& B = Type == Colossus.Name ? Colossus : Type == WardenB.Name ? WardenB : Sentinel;
	const float K = FMath::Max(0.25f, RunLevel / float(B.Level));
	AAerondelMob* M = SpawnTunedMob(B.Name, Pos, B.Name, FMath::RoundToInt(B.Hp * K * (bBoss ? 1.4f : 1.f)), FMath::Max(4, FMath::RoundToInt(B.MaxHit * FMath::Sqrt(K))),
		FMath::Max(1, FMath::RoundToInt(B.Att * K)), FMath::Max(1, FMath::RoundToInt(B.Def * K)), bBoss ? RunLevel + 15 : RunLevel, B.Scale, bBoss);
	if (!M) return;
	Live.Add(M);
	if (bBoss) Boss = M; else if (Type == WardenB.Name) Warden = M;
}

FString URelicryFeature::EnterVault(int32 Site)
{
	const RelicryData::FSite* S = SiteAt(Site); if (!S) return FString();
	if (bInRun) return TEXT("You are already in a dungeon.");
	const TCHAR* Art = nullptr; for (const RelicryData::FArt& A : S->Arts) if (!Art && CountItem(A.Name) > 0) Art = A.Name;
	if (!Art || !TakeItem(Art, 1)) return FString::Printf(TEXT("The vault is sealed. Restore an artifact from the %s to open it."), S->Name);
	bInRun = true; bBossDead = false; bChest = false; RunSite = Site; RunLevel = DungeonLevel(Site); RunTicks = 0; DigSite = -1;
	const float Ease = UAerondelRules::Perk(GetPS(), TEXT("relicry"), TEXT("gateEase"));
	Gates.Reset(); int32 i = 0;
	for (const FString& Sk : AerondelGates::Draw(Seen, NumGates))
	{
		FRelicGate G; G.Skill = Sk; G.Need = AerondelGates::GateLevel(RunLevel, i++, Ease);
		if (const RelicryData::FGate* D = AerondelGates::Find(Sk)) G.Name = D->Name;
		Gates.Add(G); Seen.FindOrAdd(Sk) += 1;
	}
	// The run is staged in the vault courtyard (web x-5, z+5 from the site centre): sentinels, then the colossus beyond the gates.
	const FVector2D V(S->X - 5.f, S->Z + 5.f);
	for (int32 j = 0; j < (RunLevel >= 50 ? 3 : 2); ++j) SpawnScaled(Sentinel.Name, V + FVector2D(-3.f + j * 3.f, 8.f), false);
	SpawnScaled(Colossus.Name, V + FVector2D(0.f, 18.f), true);
	FString List; for (const FRelicGate& G : Gates) List += (List.IsEmpty() ? TEXT("") : TEXT(", ")) + G.Skill;
	return FString::Printf(TEXT("The %s fits the seal. Relic dungeon, level %d: %s gates, then the Relic colossus."), Art, RunLevel, *List);
}

FString URelicryFeature::TryGate(int32 Index, bool bUseKey)
{
	if (!bInRun || !Gates.IsValidIndex(Index)) return FString();
	for (int32 i = 0; i < Index; ++i) if (!Gates[i].bOpen) return TEXT("An earlier gate still blocks the way.");
	FRelicGate& G = Gates[Index]; if (G.bOpen) return TEXT("It is already open.");
	if (Warden.IsValid() && !Warden->IsDead()) return TEXT("The warden blocks the way. Beat it!");
	if (bUseKey) { if (!TakeItem(TEXT("Relic key"), 1)) return TEXT("You have no Relic key."); G.bOpen = true; return TEXT("The Relic key turns and the gate opens."); }
	if (SkillLevel(G.Skill) < G.Need) return FString::Printf(TEXT("You need %s %d. A Relic key would open it, or come back stronger."), *G.Skill, G.Need);
	const RelicryData::FGate* D = AerondelGates::Find(G.Skill);
	const FString Kind = D ? D->Kind : TEXT("");
	if (Kind == TEXT("fight"))
	{
		const RelicryData::FSite* S = SiteAt(RunSite); FVector2D P; if (!PlayerWebPos(P) && S) P = FVector2D(S->X, S->Z);
		SpawnScaled(WardenB.Name, P + FVector2D(2.f, 2.f), false);
		return TEXT("A Vault warden steps out of the wall!");
	}
	if (Kind == TEXT("hp"))
	{
		AAerondelPlayerState* PS = GetPS(); if (!PS || !PS->Health) return FString();
		const int32 Cost = FMath::Max(1, FMath::RoundToInt(PS->Health->GetMax() * 0.1f));
		if (PS->Health->GetCurrent() <= Cost + 1) return TEXT("You are too weak to feed the seal.");
		PS->Health->ApplyDamage(Cost, nullptr);
	}
	GiveXp(G.Skill, 20.f + G.Need * 6.f); GiveXp(TEXT("Relicry"), 10.f + G.Need * 2.f);
	G.bOpen = true;
	return D ? FString(D->Done) : FString(TEXT("The gate opens."));
}

FString URelicryFeature::OpenChest()
{
	if (!bInRun) return FString();
	if (!bBossDead) return TEXT("The chest is sealed by the guardian's power.");
	if (bChest) return TEXT("Empty.");
	bChest = true; const int32 L = RunLevel; const RelicryData::FSite* S = SiteAt(RunSite);
	GiveCoins(60 * L + FMath::RandHelper(40 * L)); GiveItem(TEXT("Relic shard"), 5 + FMath::RandHelper(10));
	GiveItem(TEXT("Ancient fragment"), 1 + FMath::RandHelper(1 + L / 30));
	if (S) GiveItem(S->Mat, 4 + L / 10);
	if (FMath::FRand() < 0.3f) GiveItem(TEXT("Relic key"), 1);
	GiveXp(TEXT("Relicry"), 200.f + L * 15.f);
	FString Gear;
	if (FMath::FRand() < 0.02f + L / 2500.f) { Gear = RelicryData::RelicGear[FMath::RandHelper((int32)UE_ARRAY_COUNT(RelicryData::RelicGear))]; GiveItem(Gear, 1); }
	++Runs; const int32 Secs = FMath::RoundToInt(RunTicks * 0.6f); if (!Best || Secs < Best) Best = Secs;
	return FString::Printf(TEXT("Inside: coins, shards, fragments%s. Run %d done in %ds."), Gear.IsEmpty() ? TEXT("") : *(TEXT(" and the ") + Gear), Runs, Secs);
}

void URelicryFeature::Finish(bool bWin)
{
	if (!bInRun) return;
	bInRun = false;
	for (const TWeakObjectPtr<AAerondelMob>& M : Live) if (M.IsValid()) M->Destroy();
	Live.Reset(); Boss.Reset(); Warden.Reset(); Gates.Reset();
	if (!bWin) Say(TEXT("You leave the relic dungeon."));
}
