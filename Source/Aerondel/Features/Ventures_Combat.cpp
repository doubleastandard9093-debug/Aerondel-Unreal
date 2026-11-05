#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/AerondelMob.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Interaction/BankChest.h"
#include "Data/ItemDef.h"
#include "Data/MobDef.h"
#include "Dom/JsonValue.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// ============================================================================================
// Ventures group 2 (web adapt/crestbreak.js, sigils.js, grafts.js, twinstance.js, loot.js):
// Crestbreak, Engrave Sigils, Riven Grafts, Twin Stance and the shared Ventures loot.
// ============================================================================================

namespace VenturesCombat
{
	struct FSigil { const TCHAR* Key; const TCHAR* Name; const TCHAR* What; float V[3]; };
	static const FSigil Sigils[] = {
		{ TEXT("ambush"), TEXT("Ambush"), TEXT("damage on a foe at full health"), { 0.25f, 0.4f, 0.6f } },
		{ TEXT("ricochet"), TEXT("Ricochet Master"), TEXT("chakram hits bounce to extra foes"), { 1.f, 2.f, 3.f } },
		{ TEXT("keen"), TEXT("Keen Eye"), TEXT("accuracy"), { 0.04f, 0.07f, 0.1f } },
		{ TEXT("grudge"), TEXT("Grudgebearer"), TEXT("damage against bosses"), { 0.08f, 0.12f, 0.16f } },
		{ TEXT("bulwark"), TEXT("Bulwark"), TEXT("less damage taken"), { 0.04f, 0.06f, 0.08f } },
		{ TEXT("surge"), TEXT("Surging Soul"), TEXT("Overflare gain"), { 0.15f, 0.25f, 0.35f } },
		{ TEXT("breaker"), TEXT("Crestbreaker"), TEXT("stagger built"), { 0.2f, 0.35f, 0.5f } },
		{ TEXT("scholar"), TEXT("Battle Scholar"), TEXT("combat XP"), { 0.05f, 0.08f, 0.12f } },
		{ TEXT("cadence"), TEXT("Blade Cadence"), TEXT("damage with katanas and scimitars"), { 0.06f, 0.09f, 0.12f } },
		{ TEXT("arcanum"), TEXT("Arcanum"), TEXT("Magic damage"), { 0.06f, 0.09f, 0.12f } },
		{ TEXT("deadeye"), TEXT("Deadeye"), TEXT("Ranged damage"), { 0.06f, 0.09f, 0.12f } },
	};
	static const FSigil* FindSigil(const FString& K)
	{
		for (const FSigil& S : Sigils) if (K == S.Key) return &S;
		return nullptr;
	}
	static const TCHAR* GraftTypes[] = { TEXT("keen"), TEXT("brutal"), TEXT("warding"), TEXT("fervent") };
	static const TCHAR* GraftStats[] = { TEXT("acc"), TEXT("dmg"), TEXT("dr"), TEXT("flare") };
	static const TCHAR* GraftTiers[] = { TEXT("Lesser"), TEXT("Greater"), TEXT("Exalted") };
	static const float GraftVals[] = { 0.02f, 0.05f, 0.09f };
	static const TCHAR* Stances[] = { TEXT("Melee"), TEXT("Ranged"), TEXT("Magic"), TEXT("Swordsmanship") };

	/** "Lesser keen graft" -> tier 0, type 0. */
	static bool ParseGraft(const FString& N, int32& Tier, int32& Type)
	{
		TArray<FString> P; N.ParseIntoArray(P, TEXT(" "));
		if (P.Num() != 3 || P[2] != TEXT("graft")) return false;
		Tier = Type = -1;
		for (int32 i = 0; i < 3; ++i) if (P[0] == GraftTiers[i]) Tier = i;
		for (int32 i = 0; i < 4; ++i) if (P[1] == GraftTypes[i]) Type = i;
		return Tier >= 0 && Type >= 0;
	}
	static bool IsStance(const FString& S)
	{
		for (const TCHAR* X : Stances) if (S == X) return true;
		return false;
	}
	static bool StanceHasSkill(const FString& St, FName Skill)
	{
		if (St == TEXT("Melee")) return Skill == FName(TEXT("attack")) || Skill == FName(TEXT("strength"));
		if (St == TEXT("Ranged")) return Skill == FName(TEXT("ranged"));
		if (St == TEXT("Magic")) return Skill == FName(TEXT("magic"));
		if (St == TEXT("Swordsmanship")) return Skill == FName(TEXT("swordsmanship"));
		return false;
	}
	static const TCHAR* VenturesRomanTier(int32 T) { return T >= 3 ? TEXT("III") : T == 2 ? TEXT("II") : T == 1 ? TEXT("I") : TEXT("-"); }
}
using namespace VenturesCombat;

static void VAct2(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true)
{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; M.Actions.Add(MoveTemp(A)); }

static FString ItemName(const UItemDef* I) { return I ? I->DisplayName.ToString() : FString(); }

// ---------------------------------------------------------------- state

void UVenturesFeature::ResetCombat()
{
	Crest.Reset();
	CrestBreaks = 0;
	bBouncing = false;
	SigRead.Reset();
	SigOn.Reset();
	GraftsOn.Reset();
	GraftSel.Reset();
	StanceMain.Reset();
	StanceSub.Reset();
}

void UVenturesFeature::SaveCombat(FJsonObject& A) const
{
	TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
	C->SetNumberField(TEXT("breaks"), CrestBreaks);
	A.SetObjectField(TEXT("crest"), C);

	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>(), R = MakeShared<FJsonObject>();
	for (const auto& E : SigRead) R->SetNumberField(FString(*E.Key), E.Value);
	S->SetObjectField(TEXT("read"), R);
	PutStrArr(*S, TEXT("on"), SigOn);
	A.SetObjectField(TEXT("sigils"), S);

	TSharedRef<FJsonObject> G = MakeShared<FJsonObject>(), On = MakeShared<FJsonObject>();
	for (const auto& E : GraftsOn) PutStrArr(*On, *E.Key, E.Value);
	G->SetObjectField(TEXT("on"), On);
	A.SetObjectField(TEXT("grafts"), G);

	TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
	if (StanceMain.IsEmpty()) T->SetField(TEXT("main"), MakeShared<FJsonValueNull>()); else T->SetStringField(TEXT("main"), StanceMain);
	if (StanceSub.IsEmpty()) T->SetField(TEXT("sub"), MakeShared<FJsonValueNull>()); else T->SetStringField(TEXT("sub"), StanceSub);
	A.SetObjectField(TEXT("stance"), T);
}

void UVenturesFeature::LoadCombat(const FJsonObject& A)
{
	if (const TSharedPtr<FJsonObject> C = JObj(A, TEXT("crest"))) CrestBreaks = FMath::Max(0, (int32)JInt(*C, TEXT("breaks")));
	if (const TSharedPtr<FJsonObject> S = JObj(A, TEXT("sigils")))
	{
		if (const TSharedPtr<FJsonObject> R = JObj(*S, TEXT("read")))
			for (const auto& E : R->Values)
				if (FindSigil(FString(*E.Key))) SigRead.Add(FString(*E.Key), FMath::Clamp((int32)JInt(*R, *E.Key), 0, 6));
		for (const FString& K : JStrArr(*S, TEXT("on")))
			if (FindSigil(K) && !SigOn.Contains(K) && SigOn.Num() < 5) SigOn.Add(K);
	}
	if (const TSharedPtr<FJsonObject> G = JObj(A, TEXT("grafts")))
		if (const TSharedPtr<FJsonObject> On = JObj(*G, TEXT("on")))
			for (const auto& E : On->Values)
			{
				TArray<FString> Ok;
				int32 Ti, Ty;
				for (const FString& N : JStrArr(*On, *E.Key)) if (ParseGraft(N, Ti, Ty) && Ok.Num() < 4) Ok.Add(N);
				if (Ok.Num()) GraftsOn.Add(FString(*E.Key), Ok);
			}
	if (const TSharedPtr<FJsonObject> T = JObj(A, TEXT("stance")))
	{
		const FString Ma = JStr(*T, TEXT("main")), Su = JStr(*T, TEXT("sub"));
		StanceMain = IsStance(Ma) ? Ma : FString();
		StanceSub = IsStance(Su) && Su != StanceMain ? Su : FString();
	}
}

// ---------------------------------------------------------------- Crestbreak

bool UVenturesFeature::IsBroken(const AAerondelMob* Mob) const
{
	const FCrestState* C = Mob ? Crest.Find(TWeakObjectPtr<AAerondelMob>(const_cast<AAerondelMob*>(Mob))) : nullptr;
	return C && TickN < C->BrokeUntil;
}

float UVenturesFeature::StaggerOf(const AAerondelMob* Mob) const
{
	const FCrestState* C = Mob ? Crest.Find(TWeakObjectPtr<AAerondelMob>(const_cast<AAerondelMob*>(Mob))) : nullptr;
	return C ? C->Stag : 0.f;
}

void UVenturesFeature::Stagger(AAerondelMob* Mob, float N)
{
	if (!Mob || Mob->IsDead() || !IsBossy(Mob) || IsBroken(Mob)) return;
	FCrestState& C = Crest.FindOrAdd(TWeakObjectPtr<AAerondelMob>(Mob));
	C.Stag += N * (1.f + Sig(TEXT("breaker")) + StagGear());
	if (C.Stag < 100.f) return;
	C.Stag = 0.f;
	C.Parts++;
	C.BrokeUntil = TickN + 8;
	Mob->Stun(3);
	CrestBreaks++;
	const FString MName = Mob->Definition ? Mob->Definition->DisplayName.ToString() : FString(TEXT("foe"));
	const FString L = MName.ToLower();
	const bool bWyrm = L.Contains(TEXT("wyrm")) || L.Contains(TEXT("drake")) || L.Contains(TEXT("dragon"));
	const bool bStone = L.Contains(TEXT("golem")) || L.Contains(TEXT("colossus")) || L.Contains(TEXT("stonehorn")) || L.Contains(TEXT("frost"));
	const int32 Pi = FMath::Min(1, C.Parts - 1);
	const TCHAR* Part = bWyrm ? (Pi ? TEXT("scale-heart") : TEXT("horn")) : bStone ? (Pi ? TEXT("core") : TEXT("crystal crest")) : (Pi ? TEXT("core") : TEXT("crest"));
	Say(FString::Printf(TEXT("Crestbreak! The %s's %s shatters. It is stunned and takes bonus damage."), *MName, Part));
	FlareAdd(15.f);
}

int32 UVenturesFeature::HitCombat(AAerondelMob* Mob, int32 D)
{
	if (!Mob || D <= 0) return D;
	const bool bFull = Mob->GetHp() >= Mob->GetMaxHp();
	// Per-foe damage that the def-only DamageDealtBonus cannot see: broken parts and Ambush.
	float Extra = (IsBroken(Mob) ? (WeaponClass() == FName(TEXT("katana")) ? 0.7f : 0.4f) : 0.f) + (bFull ? Sig(TEXT("ambush")) : 0.f);
	Extra += DmgRaidActor(Mob) + DmgSeasonActor(Mob);
	Extra = FMath::Min(Extra, CapDmg);
	if (Extra > 0.f) D = FMath::Max(1, FMath::RoundToInt(D * (1.f + Extra)));
	const bool bFlare = StrikeKind && FCString::Strcmp(StrikeKind, TEXT("flare")) == 0;
	const bool bTether = StrikeKind && FCString::Strcmp(StrikeKind, TEXT("tether")) == 0;
	const bool bBounce = StrikeKind && FCString::Strcmp(StrikeKind, TEXT("bounce")) == 0;
	if (IsBossy(Mob))
	{
		const FName Cls = WeaponClass();
		float N = 2.f + FMath::Min(6.f, D / (float)FMath::Max(1, Mob->GetMaxHp()) * 60.f);
		if (Cls == FName(TEXT("warfan")) || Cls == FName(TEXT("glaive"))) N *= 2.f;
		if (bFlare) N *= 2.5f;
		if (bTether) N += 12.f;
		Stagger(Mob, N);
	}
	const int32 Bounces = FMath::RoundToInt(Sig(TEXT("ricochet")));
	if (Bounces > 0 && !bBouncing && !bBounce && WeaponClass() == FName(TEXT("chakram")))
	{
		bBouncing = true;
		const FVector L = Mob->GetActorLocation();
		int32 Done = 0;
		for (AAerondelMob* O : MobsNear(FVector2D(L.X / 100.0, L.Y / 100.0), 6.f, Bounces + 1))
		{
			if (O == Mob || Done >= Bounces) continue;
			Strike(O, 0.f, -0.4f, TEXT("bounce"));
			++Done;
		}
		bBouncing = false;
	}
	HitSeason(Mob, D);
	return HitRaid(Mob, D);
}

void UVenturesFeature::DiedCombat(AAerondelMob* Mob, bool bByPlayer)
{
	const TWeakObjectPtr<AAerondelMob> K(Mob);
	if (const FCrestState* C = Crest.Find(K))
	{
		const int32 Parts = FMath::Min(2, C->Parts);
		for (int32 i = 0; i < Parts; ++i) BonusDrop(Mob, i ? TEXT("the broken core") : TEXT("the broken crest"));
		Crest.Remove(K);
	}
	if (bByPlayer && IsBossy(Mob) && FMath::FRand() < 0.15f)
		BonusDrop(Mob, FString::Printf(TEXT("the %s"), Mob->Definition ? *Mob->Definition->DisplayName.ToString() : TEXT("foe")));
	// drop dead weak keys
	for (auto It = Crest.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
}

void UVenturesFeature::BonusDrop(const AAerondelMob* Mob, const FString& Why)
{
	const bool bBig = Mob && Mob->Definition && Mob->Definition->CombatLevel >= 200;
	FString It;
	if (FMath::FRand() < 0.4f) It = TEXT("Sigil tome");
	else It = FString::Printf(TEXT("%s %s graft"), bBig && FMath::FRand() < 0.3f ? TEXT("Greater") : TEXT("Lesser"), GraftTypes[FMath::RandRange(0, 3)]);
	GiveItem(It, 1);
	Say(FString::Printf(TEXT("Bonus from %s: %s."), *Why, *It));
}

// ---------------------------------------------------------------- Sigils

int32 UVenturesFeature::SigTier(const FString& Key) const
{
	const int32* N = SigRead.Find(Key);
	const int32 V = N ? *N : 0;
	return V >= 6 ? 3 : V >= 3 ? 2 : V >= 1 ? 1 : 0;
}

float UVenturesFeature::Sig(const FString& Key) const
{
	if (!SigOn.Contains(Key)) return 0.f;
	const int32 T = SigTier(Key);
	const FSigil* S = FindSigil(Key);
	return S && T ? S->V[T - 1] : 0.f;
}

void UVenturesFeature::Study(const FString& Key)
{
	const FSigil* S = FindSigil(Key);
	if (!S) return;
	int32& N = SigRead.FindOrAdd(Key);
	if (N >= 6) { Say(TEXT("That sigil is already at tier III.")); return; }
	if (!TakeItem(TEXT("Sigil tome"), 1)) { Say(TEXT("You need a Sigil tome (bosses, broken parts, or Glyphcraft 30: 25 Havoc runes + 2 Soft leather).")); return; }
	const int32 T0 = SigTier(Key);
	N++;
	const int32 T = SigTier(Key);
	static const int32 Need[] = { 1, 3, 6, 6 };
	if (T > T0) Say(FString::Printf(TEXT("You engrave %s %s."), S->Name, VenturesRomanTier(T)));
	else Say(FString::Printf(TEXT("You study %s (%d/%d toward the next tier)."), S->Name, N, Need[T]));
}

void UVenturesFeature::SigToggle(const FString& Key)
{
	if (SigOn.Contains(Key)) SigOn.Remove(Key);
	else if (!SigTier(Key)) Say(TEXT("Study a tome for this sigil first."));
	else if (SigOn.Num() >= 5) Say(TEXT("All 5 sigil slots are full. Remove one first."));
	else SigOn.Add(Key);
}

// ---------------------------------------------------------------- Grafts

float UVenturesFeature::GraftSum(const FString& Stat) const
{
	const AAerondelPlayerState* PS = GetPS();
	if (!PS || !PS->Equipment) return 0.f;
	float T = 0.f;
	for (const auto& E : PS->Equipment->GetAll())
	{
		const TArray<FString>* L = GraftsOn.Find(ItemName(E.Value.Get()));
		if (!L) continue;
		int32 Ti, Ty;
		for (const FString& G : *L) if (ParseGraft(G, Ti, Ty) && Stat == GraftStats[Ty]) T += GraftVals[Ti];
	}
	return T;
}

static bool IsWorn(const AAerondelPlayerState* PS, const FString& Item)
{
	if (!PS || !PS->Equipment) return false;
	for (const auto& E : PS->Equipment->GetAll()) if (ItemName(E.Value.Get()) == Item) return true;
	return false;
}

bool UVenturesFeature::GraftOn(const FString& Item, const FString& Graft)
{
	if (!IsWorn(GetPS(), Item)) { Say(TEXT("Wear the item first.")); return false; }
	TArray<FString>& L = GraftsOn.FindOrAdd(Item);
	if (L.Num() >= 4) { Say(TEXT("That item has no free graft slots (4 max).")); return false; }
	int32 Ti, Ty;
	if (!ParseGraft(Graft, Ti, Ty) || !TakeItem(Graft, 1)) { if (!L.Num()) GraftsOn.Remove(Item); return false; }
	L.Add(Graft);
	Say(FString::Printf(TEXT("%s grafted onto your %s."), *Graft, *Item));
	return true;
}

void UVenturesFeature::GraftOff(const FString& Item, int32 Index)
{
	TArray<FString>* L = GraftsOn.Find(Item);
	if (!L || !L->IsValidIndex(Index)) return;
	if (!TakeCoins(500)) { Say(TEXT("Removing a graft costs 500 coins.")); return; }
	GiveItem((*L)[Index], 1);
	L->RemoveAt(Index);
	if (!L->Num()) GraftsOn.Remove(Item);
}

void UVenturesFeature::GraftFuse(const FString& Graft)
{
	int32 Ti, Ty;
	if (!ParseGraft(Graft, Ti, Ty) || Ti >= 2) { Say(TEXT("Only Lesser and Greater grafts can be fused.")); return; }
	if (CountItem(Graft) < 3) { Say(TEXT("Fusing takes 3 matching grafts.")); return; }
	TakeItem(Graft, 3);
	const float Chance = 0.65f + FMath::Min(0.25f, SkillLevel(TEXT("Smithing")) / 400.f);
	if (FMath::FRand() < Chance)
	{
		const FString Out = FString::Printf(TEXT("%s %s graft"), GraftTiers[Ti + 1], GraftTypes[Ty]);
		GiveItem(Out, 1);
		Say(FString::Printf(TEXT("Fusion holds: %s!"), *Out));
	}
	else { GiveItem(Graft, 1); Say(TEXT("The fusion cracks. One graft survives.")); }
	GiveXp(TEXT("Smithing"), 80.f * (Ti + 1));
}

// ---------------------------------------------------------------- Twin Stance

bool UVenturesFeature::NearBank() const
{
	const AAerondelPlayerState* PS = GetPS(); const APawn* P = PS ? PS->GetPawn() : nullptr;
	UWorld* W = P ? P->GetWorld() : nullptr;
	if (!W) return false;
	for (TActorIterator<ABankChest> It(W); It; ++It)
		if (FVector::Dist2D(It->GetActorLocation(), P->GetActorLocation()) < 800.f) return true;
	return false;
}

void UVenturesFeature::SetStance(const FString& Slot, const FString& St)
{
	if (!NearBank()) { Say(TEXT("You can only change stance beside a bank.")); return; }
	if (!IsStance(St)) return;
	const bool bMain = Slot == TEXT("main");
	(bMain ? StanceMain : StanceSub) = St;
	if (StanceMain == StanceSub) (bMain ? StanceSub : StanceMain).Reset();
	Say(FString::Printf(TEXT("Stance: %s with %s support."), StanceMain.IsEmpty() ? TEXT("-") : *StanceMain, StanceSub.IsEmpty() ? TEXT("-") : *StanceSub));
}

bool UVenturesFeature::StanceOn() const
{
	return !StanceMain.IsEmpty() && !StanceSub.IsEmpty() && Style() == StanceMain;
}

float UVenturesFeature::PerkBoost(const UObject* WorldContext, FName SkillId)
{
	const UVenturesFeature* V = Get(WorldContext);
	if (!V) return 0.f;
	return (StanceHasSkill(V->StanceMain, SkillId) ? 0.2f : 0.f) + (StanceHasSkill(V->StanceSub, SkillId) ? 0.1f : 0.f);
}

// ---------------------------------------------------------------- totals

float UVenturesFeature::DmgCombat(const UMobDef* Target) const
{
	float T = GraftSum(TEXT("dmg")) + (StanceOn() ? 0.04f : 0.f);
	if (Target)
	{
		bool bBossy = Target->bBoss;
		static const TCHAR* Words[] = { TEXT("Warden"), TEXT("Colossus"), TEXT("Behemoth"), TEXT("Warlord"), TEXT("Revenant"), TEXT("Tyrant"), TEXT("Archon"), TEXT("Knight"), TEXT("Sunderlord") };
		const FString N = Target->DisplayName.ToString();
		if (!bBossy && Target->CombatLevel >= 140) for (const TCHAR* Wd : Words) if (N.Contains(Wd)) bBossy = true;
		if (bBossy) T += Sig(TEXT("grudge"));
	}
	const FName Cls = WeaponClass();
	if (Cls == FName(TEXT("katana")) || Cls == FName(TEXT("scimitar"))) T += Sig(TEXT("cadence"));
	const FString St = Style();
	if (St == TEXT("Magic")) T += Sig(TEXT("arcanum"));
	if (St == TEXT("Ranged")) T += Sig(TEXT("deadeye"));
	return T;
}

float UVenturesFeature::AccCombat() const { return GraftSum(TEXT("acc")) + Sig(TEXT("keen")) + (StanceOn() ? 0.04f : 0.f); }
float UVenturesFeature::DrCombat() const { return GraftSum(TEXT("dr")) + Sig(TEXT("bulwark")); }
float UVenturesFeature::FlareCombat() const { return GraftSum(TEXT("flare")) * 4.f + Sig(TEXT("surge")); }

float UVenturesFeature::XpCombat(FName SkillId) const
{
	static const FName Combat[] = { FName(TEXT("attack")), FName(TEXT("strength")), FName(TEXT("defence")), FName(TEXT("ranged")), FName(TEXT("magic")), FName(TEXT("swordsmanship")), FName(TEXT("hitpoints")) };
	for (const FName& C : Combat) if (C == SkillId) return Sig(TEXT("scholar"));
	return 0.f;
}

// ---------------------------------------------------------------- tabs

void UVenturesFeature::TabCrest(FGPanelModel& M)
{
	M.Title = TEXT("Crestbreak");
	M.Lines.Add(TEXT("Bosses have a stagger bar. Fill it to break a part: the boss is stunned, takes +40% damage (+70% with a katana) for 8 ticks, and drops an extra reward on death for each broken part (up to 2)."));
	M.Lines.Add(FString::Printf(TEXT("Stagger builders: war fans and glaives x2, Overflare x2.5, Tetherline strikes. Parts broken so far: %d."), CrestBreaks));
	if (AAerondelMob* T = CurrentTarget())
		if (IsBossy(T))
			M.Lines.Add(IsBroken(T) ? FString(TEXT("Target: BROKEN - hit it now!")) : FString::Printf(TEXT("Target stagger: %d%%"), FMath::RoundToInt(StaggerOf(T))));
}

void UVenturesFeature::TabSigils(FGPanelModel& M)
{
	M.Title = FString::Printf(TEXT("Sigils %d/5 - tomes held %d"), SigOn.Num(), CountItem(TEXT("Sigil tome")));
	for (const FSigil& S : Sigils)
	{
		const FString K = S.Key;
		const int32 T = SigTier(K);
		const bool bOn = SigOn.Contains(K);
		auto Val = [](float X) { return X < 1.f ? FString::Printf(TEXT("+%d%%"), FMath::RoundToInt(X * 100.f)) : FString::Printf(TEXT("+%d"), FMath::RoundToInt(X)); };
		M.Lines.Add(FString::Printf(TEXT("%s %s: %s / %s / %s %s"), S.Name, T ? VenturesRomanTier(T) : TEXT("(unlearned)"), *Val(S.V[0]), *Val(S.V[1]), *Val(S.V[2]), S.What));
		VAct2(M, FString::Printf(TEXT("Study %s"), S.Name), [this, K] { Study(K); });
		VAct2(M, FString::Printf(TEXT("%s: %s"), S.Name, bOn ? TEXT("On") : TEXT("Off")), [this, K] { SigToggle(K); }, T > 0 || bOn);
	}
	M.Lines.Add(TEXT("Tiers I-III need 1, 3 and 6 tomes."));
}

void UVenturesFeature::TabGrafts(FGPanelModel& M)
{
	M.Title = TEXT("Riven Grafts");
	const AAerondelPlayerState* PS = GetPS();
	TArray<FString> Held;
	if (PS && PS->Inventory)
		for (const FItemStack& S : PS->Inventory->GetSlots())
		{
			if (S.IsEmpty()) continue;
			const FString N = ItemName(S.Item.Get());
			int32 Ti, Ty;
			if (ParseGraft(N, Ti, Ty)) Held.AddUnique(N);
		}
	if (!Held.Contains(GraftSel)) GraftSel = Held.Num() ? Held[0] : FString();
	M.Lines.Add(Held.Num() ? FString::Printf(TEXT("Held: %s"), *FString::Join(Held, TEXT(", "))) : FString(TEXT("No grafts held. Forge Lesser grafts at an anvil (Smithing 30) or win them from bosses.")));
	if (Held.Num() > 1)
		VAct2(M, FString::Printf(TEXT("Selected: %s (next)"), *GraftSel), [this, Held] { const int32 i = Held.IndexOfByKey(GraftSel); GraftSel = Held[(i + 1) % Held.Num()]; });
	if (!GraftSel.IsEmpty())
	{
		const FString G = GraftSel;
		VAct2(M, FString::Printf(TEXT("Fuse 3 %s (x%d)"), *G, CountItem(G)), [this, G] { GraftFuse(G); }, CountItem(G) >= 3);
	}
	if (PS && PS->Equipment)
		for (const auto& E : PS->Equipment->GetAll())
		{
			const FString It = ItemName(E.Value.Get());
			if (It.IsEmpty()) continue;
			const TArray<FString>* L = GraftsOn.Find(It);
			const int32 Num = L ? L->Num() : 0;
			M.Lines.Add(FString::Printf(TEXT("%s %d/4: %s"), *It, Num, Num ? *FString::Join(*L, TEXT(", ")) : TEXT("empty")));
			if (!GraftSel.IsEmpty() && Num < 4) { const FString G = GraftSel; VAct2(M, FString::Printf(TEXT("Socket %s into %s"), *G, *It), [this, It, G] { GraftOn(It, G); }); }
			if (Num) VAct2(M, FString::Printf(TEXT("Remove last graft from %s (500 coins)"), *It), [this, It, Num] { GraftOff(It, Num - 1); });
		}
	M.Lines.Add(FString::Printf(TEXT("Now: +%d%% damage, +%d%% accuracy, %d%% less damage taken, +%d%% Overflare gain."),
		FMath::RoundToInt(GraftSum(TEXT("dmg")) * 100.f), FMath::RoundToInt(GraftSum(TEXT("acc")) * 100.f), FMath::RoundToInt(GraftSum(TEXT("dr")) * 100.f), FMath::RoundToInt(GraftSum(TEXT("flare")) * 400.f)));
	M.Lines.Add(TEXT("Fusion: 3 alike -> next tier, 65-90% chance with Smithing."));
}

void UVenturesFeature::TabStance(FGPanelModel& M)
{
	const bool bBank = NearBank();
	M.Title = FString::Printf(TEXT("Twin Stance - Main: %s, Support: %s"), StanceMain.IsEmpty() ? TEXT("-") : *StanceMain, StanceSub.IsEmpty() ? TEXT("-") : *StanceSub);
	if (!bBank) M.Lines.Add(TEXT("Stand beside a bank to change stance."));
	M.Lines.Add(TEXT("Main discipline perks +20%. Support discipline perks +10%, and +4% accuracy and damage while you fight in your main style."));
	M.Lines.Add(StanceOn() ? FString(TEXT("Right now: active (+4% accuracy and damage).")) : FString(TEXT("Right now: not active - use your main style.")));
	for (const TCHAR* S : Stances)
	{
		const FString St = S;
		VAct2(M, FString::Printf(TEXT("Main: %s%s"), S, StanceMain == St ? TEXT(" *") : TEXT("")), [this, St] { SetStance(TEXT("main"), St); }, bBank);
	}
	for (const TCHAR* S : Stances)
	{
		const FString St = S;
		VAct2(M, FString::Printf(TEXT("Support: %s%s"), S, StanceSub == St ? TEXT(" *") : TEXT("")), [this, St] { SetStance(TEXT("sub"), St); }, bBank);
	}
}
