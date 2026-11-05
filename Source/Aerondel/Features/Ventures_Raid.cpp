#include "Features/VenturesFeature.h"
#include "Features/CompanionsFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/AerondelMob.h"
#include "Combat/HealthComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Data/ItemDef.h"
#include "Data/MobDef.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ============================================================================================
// Ventures group 5 (web adapt/vigil.js, attune.js, hearthhold.js):
// Vigil Gates, Elemental Attunements, Hearthhold.
// ============================================================================================

namespace VenturesRaid
{
	struct FGate { const TCHAR* Key; const TCHAR* Name; const TCHAR* Boss; const TCHAR* Adds; int32 Min; };
	static const FGate Gates[] = {
		{ TEXT("vexmar"), TEXT("Vexmar's Vigil"), TEXT("Sunderlord Vexmar"), TEXT("Voidborn Knight"), 120 },
		{ TEXT("archon"), TEXT("Archon's Vigil"), TEXT("Molten Archon"), TEXT("Ember Imp"), 60 },
	};
	static const FGate* FindGate(const FString& K) { for (const FGate& G : Gates) if (K == G.Key) return &G; return nullptr; }
	static int32 VigilWeek() { return (int32)FMath::FloorToDouble((FDateTime::UtcNow().ToUnixTimestamp() * 1000.0 / 86400000.0 + 3.0) / 7.0); }

	struct FFrag { const TCHAR* Key; const TCHAR* Name; const TCHAR* Desc; };
	struct FElem { const TCHAR* Key; const TCHAR* Name; const TCHAR* Super; const TCHAR* Move; FFrag Frags[3]; const TCHAR* Gear[4]; };
	static const FElem Elems[] = {
		{ TEXT("ember"), TEXT("Ember"), TEXT("Pyre Bloom"), TEXT("Kindled blows: +5% damage"),
			{ { TEXT("kindling"), TEXT("Kindling"), TEXT("kills heal 2% health") }, { TEXT("searing"), TEXT("Searing"), TEXT("+6% damage to bosses") }, { TEXT("blaze"), TEXT("Blaze"), TEXT("Pyre Bloom hits 2 more times") } },
			{ TEXT("ember"), TEXT("cinder"), TEXT("flame"), TEXT("fire") } },
		{ TEXT("rime"), TEXT("Rime"), TEXT("Winter Seal"), TEXT("Frost skin: 5% less damage taken"),
			{ { TEXT("frostbite"), TEXT("Frostbite"), TEXT("10% of hits freeze a non-boss foe") }, { TEXT("glacial"), TEXT("Glacial"), TEXT("4% more damage reduction") }, { TEXT("shatter"), TEXT("Shatter"), TEXT("+15% damage to frozen foes") } },
			{ TEXT("rime"), TEXT("frost"), TEXT("glacier"), TEXT("ice") } },
		{ TEXT("storm"), TEXT("Storm"), TEXT("Thunderline"), TEXT("Static hands: +5% accuracy"),
			{ { TEXT("arc"), TEXT("Arc chain"), TEXT("15% of hits arc to a nearby foe") }, { TEXT("static"), TEXT("Static"), TEXT("+5% more accuracy") }, { TEXT("surge"), TEXT("Surge"), TEXT("Thunderline chains to 3 more foes") } },
			{ TEXT("storm"), TEXT("thunder"), TEXT("gale"), TEXT("sky") } },
		{ TEXT("void"), TEXT("Void"), TEXT("Hollow Well"), TEXT("Void hunger: +4% combat XP"),
			{ { TEXT("orb"), TEXT("Void orb"), TEXT("kills add 5% Overflare") }, { TEXT("hollow"), TEXT("Hollow"), TEXT("+6% more combat XP") }, { TEXT("well"), TEXT("Deep well"), TEXT("Hollow Well heals 25% instead of 12%") } },
			{ TEXT("void"), TEXT("null"), TEXT("eclipse"), TEXT("shadow") } },
	};
	static const FElem* FindElem(const FString& K) { for (const FElem& E : Elems) if (K == FString(*E.Key)) return &E; return nullptr; }
	static bool HasFragDef(const FElem& E, const FString& F) { for (const FFrag& X : E.Frags) if (F == X.Key) return true; return false; }
	static constexpr int64 SuperCdMs = 90000;

	struct FCost { int32 Gold; const TCHAR* I1; int32 N1; const TCHAR* I2; int32 N2; };
	struct FBuilding { const TCHAR* Key; const TCHAR* Name; FCost Cost[3]; const TCHAR* Desc[4]; };
	static const FBuilding Buildings[] = {
		{ TEXT("forge"), TEXT("Forge"), { { 5000, TEXT("Iron bar"), 20, nullptr, 0 }, { 25000, TEXT("Iron bar"), 60, TEXT("Coal"), 60 }, { 100000, TEXT("Iron bar"), 150, TEXT("Cobalt ore"), 20 } },
			{ TEXT("nothing"), TEXT("a Lesser graft"), TEXT("a Lesser graft and a Sigil tome"), TEXT("a Greater graft and a Sigil tome") } },
		{ TEXT("garden"), TEXT("Garden"), { { 3000, TEXT("Logs"), 30, nullptr, 0 }, { 15000, TEXT("Oak logs"), 60, nullptr, 0 }, { 60000, TEXT("Maple logs"), 100, nullptr, 0 } },
			{ TEXT("no"), TEXT("6"), TEXT("12"), TEXT("20") } },
		{ TEXT("kennel"), TEXT("Kennel"), { { 4000, TEXT("Logs"), 40, nullptr, 0 }, { 20000, TEXT("Willow logs"), 60, nullptr, 0 }, { 80000, TEXT("Yew logs"), 80, nullptr, 0 } },
			{ TEXT("x1"), TEXT("x1.5"), TEXT("x2"), TEXT("x3") } },
	};
	static const FBuilding* FindBuilding(const FString& K) { for (const FBuilding& B : Buildings) if (K == B.Key) return &B; return nullptr; }
	struct FTrip { const TCHAR* Key; const TCHAR* Name; int32 Mins; int32 Rolls; };
	static const FTrip Trips[] = { { TEXT("short"), TEXT("Coastal forage"), 10, 1 }, { TEXT("mid"), TEXT("Ridge survey"), 30, 3 }, { TEXT("long"), TEXT("Deep frontier"), 60, 7 } };
	static const FTrip* FindTrip(const FString& K) { for (const FTrip& T : Trips) if (K == T.Key) return &T; return nullptr; }
	static const TCHAR* Herbs[] = { TEXT("Greenwort"), TEXT("Marshmint"), TEXT("Tallowroot"), TEXT("Harrowbloom"), TEXT("Toadflax"), TEXT("Ironleaf") };
	struct FLoot { const TCHAR* Item; int32 N; };
	static const FLoot HoldLoot[] = { { TEXT("Iron ore"), 6 }, { TEXT("Coal"), 6 }, { TEXT("Oak logs"), 6 }, { TEXT("Willow logs"), 5 }, { TEXT("Raw trout"), 5 }, { TEXT("Flax"), 8 } };
	static FString HoldToday() { return FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d")); }
	static FString CostText(const FCost& C)
	{
		FString S = FString::Printf(TEXT("%d coins + %d %s"), C.Gold, C.N1, C.I1);
		if (C.I2) S += FString::Printf(TEXT(", %d %s"), C.N2, C.I2);
		return S;
	}
}
using namespace VenturesRaid;

static void VAct5(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }

static FVector2D MobWeb(const AAerondelMob* M) { const FVector L = M->GetActorLocation(); return FVector2D(L.X / 100.0, L.Y / 100.0); }

// ---------------------------------------------------------------- state

void UVenturesFeature::ResetRaid()
{
	VigilLock.Reset();
	VigilClears.Reset();
	VigilNext = 0;
	bBlast = false;
	AttuneEl.Reset();
	AttuneFrags.Reset();
	AttuneSupers = 0;
	SuperReady = 0;
	bHoldOwned = false;
	HoldLv.Reset();
	HoldTrips.Reset();
	HoldForgeDay.Reset();
	HoldGardenAt = 0.0;
	HoldDone = 0;
	HoldReturn = FVector2D(0.0, 10.0);
}

void UVenturesFeature::SaveRaid(FJsonObject& A) const
{
	TSharedRef<FJsonObject> V = MakeShared<FJsonObject>(), Lock = MakeShared<FJsonObject>(), Cl = MakeShared<FJsonObject>();
	for (const auto& E : VigilLock) Lock->SetNumberField(FString(*E.Key), E.Value);
	for (const auto& E : VigilClears) Cl->SetNumberField(FString(*E.Key), E.Value);
	V->SetObjectField(TEXT("lock"), Lock);
	V->SetObjectField(TEXT("clears"), Cl);
	A.SetObjectField(TEXT("vigil"), V);

	TSharedRef<FJsonObject> At = MakeShared<FJsonObject>();
	At->SetStringField(TEXT("el"), AttuneEl);
	PutStrArr(*At, TEXT("frags"), AttuneFrags);
	At->SetNumberField(TEXT("supers"), AttuneSupers);
	A.SetObjectField(TEXT("attune"), At);

	TSharedRef<FJsonObject> H = MakeShared<FJsonObject>(), Lv = MakeShared<FJsonObject>(), Tr = MakeShared<FJsonObject>();
	H->SetBoolField(TEXT("owned"), bHoldOwned);
	for (const auto& E : HoldLv) Lv->SetNumberField(FString(*E.Key), E.Value);
	H->SetObjectField(TEXT("lv"), Lv);
	for (const auto& E : HoldTrips)
	{
		TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
		T->SetStringField(TEXT("kind"), E.Value.Kind);
		T->SetNumberField(TEXT("back"), (double)E.Value.Back);
		Tr->SetObjectField(FString(*E.Key).ToString(), T);
	}
	H->SetObjectField(TEXT("trips"), Tr);
	H->SetStringField(TEXT("forgeDay"), HoldForgeDay);
	H->SetNumberField(TEXT("garden"), HoldGardenAt);
	H->SetNumberField(TEXT("done"), HoldDone);
	A.SetObjectField(TEXT("hold"), H);
}

void UVenturesFeature::LoadRaid(const FJsonObject& A)
{
	if (const TSharedPtr<FJsonObject> V = JObj(A, TEXT("vigil")))
	{
		if (const TSharedPtr<FJsonObject> Lock = JObj(*V, TEXT("lock")))
			for (const auto& E : Lock->Values)
			{
				FString G, T;
				if (FString(*E.Key).Split(TEXT(":"), &G, &T) && FindGate(G) && (T == TEXT("normal") || T == TEXT("hard"))) VigilLock.Add(FString(*E.Key), (int32)JInt(*Lock, *E.Key));
			}
		if (const TSharedPtr<FJsonObject> Cl = JObj(*V, TEXT("clears")))
			for (const auto& E : Cl->Values) if (FindGate(FString(*E.Key))) VigilClears.Add(FString(*E.Key), FMath::Max(0, (int32)JInt(*Cl, *E.Key)));
	}
	if (const TSharedPtr<FJsonObject> At = JObj(A, TEXT("attune")))
	{
		const FString El = JStr(*At, TEXT("el"));
		if (const FElem* E = FindElem(El))
		{
			AttuneEl = El;
			for (const FString& F : JStrArr(*At, TEXT("frags"))) if (HasFragDef(*E, F) && AttuneFrags.Num() < 2) AttuneFrags.AddUnique(F);
		}
		AttuneSupers = FMath::Max(0, (int32)JInt(*At, TEXT("supers")));
	}
	if (const TSharedPtr<FJsonObject> H = JObj(A, TEXT("hold")))
	{
		bHoldOwned = JBool(*H, TEXT("owned"));
		if (const TSharedPtr<FJsonObject> Lv = JObj(*H, TEXT("lv")))
			for (const FBuilding& B : Buildings) HoldLv.Add(B.Key, FMath::Clamp((int32)JInt(*Lv, B.Key), 0, 3));
		if (const TSharedPtr<FJsonObject> Tr = JObj(*H, TEXT("trips")))
			for (const auto& E : Tr->Values)
			{
				const TSharedPtr<FJsonObject> T = JObj(*Tr, *E.Key);
				if (!T) continue;
				FHoldTrip Trip;
				Trip.Kind = JStr(*T, TEXT("kind"));
				Trip.Back = (int64)JNum(*T, TEXT("back"));
				if (FindTrip(Trip.Kind) && Trip.Back > 0) HoldTrips.Add(FName(*E.Key), Trip);
			}
		HoldForgeDay = JStr(*H, TEXT("forgeDay"));
		HoldGardenAt = FMath::Max(0.0, JNum(*H, TEXT("garden")));
		HoldDone = FMath::Max(0, (int32)JInt(*H, TEXT("done")));
	}
}

void UVenturesFeature::TickRaid()
{
	if (TickN % 33 != 0) return; // web: every 20 s (0.6 s ticks)
	const int64 Now = NowMs();
	for (auto& E : HoldTrips)
	{
		if (Now < E.Value.Back || E.Value.bTold) continue;
		E.Value.bTold = true;
		const FCompanionDef* D = UCompanionsFeature::Def(FString(*E.Key));
		Say(FString::Printf(TEXT("%s is back from an expedition (Ventures -> Hold)."), D ? *D->Name : TEXT("A companion")));
	}
}

// ---------------------------------------------------------------- Vigil Gates

bool UVenturesFeature::VigilStart(const FString& Gate, bool bHard)
{
	const FGate* G = FindGate(Gate);
	if (!G) return false;
	if (VLevel() < G->Min) { Say(FString::Printf(TEXT("%s needs an average combat level of %d."), G->Name, G->Min)); return false; }
	if (!RunStart(TEXT("vigil"), TEXT("vigil"))) return false;
	Run.Mode = Gate;
	Run.bHard = bHard;
	Run.Phase = 1;
	VigilNext = TickN + 6;
	bBlast = false;
	Run.L = FMath::RoundToInt(VLevel() * (bHard ? 1.4f : 1.05f));
	Run.Boss = ZoneSpawn(TEXT("vigil"), G->Boss, 0.f, -6.f, (float)Run.L, bHard ? 4.f : 2.5f, true);
	Say(FString::Printf(TEXT("%s%s. Watch for red rings on the ground and step out of them!"), G->Name, bHard ? TEXT(" - Hard") : TEXT("")));
	return true;
}

void UVenturesFeature::VigilStep()
{
	AAerondelMob* B = Run.Boss.Get();
	if (!B) { RunEnd(false); return; }
	if (B->IsDead()) { RunEnd(true); return; }
	const FGate* G = FindGate(Run.Mode);
	const float F = B->GetHp() / (float)FMath::Max(1, B->GetMaxHp());
	if (Run.Phase == 1 && F < 0.66f)
	{
		Run.Phase = 2;
		Say(TEXT("The Vigil's lord calls its guard!"));
		const int32 N = CapFoes(15) <= 8 ? 2 : 4;
		for (int32 i = 0; i < N && G; ++i) ZoneSpawn(TEXT("vigil"), G->Adds, -8.f + i * 5.f, -2.f, Run.L * 0.7f);
	}
	if (Run.Phase == 2 && F < 0.33f) { Run.Phase = 3; Say(TEXT("The Vigil's lord is enraged! Rings come faster.")); }
	FVector2D P;
	const bool bPos = PlayerWebPos(P);
	if (bBlast && TickN >= BlastTick)
	{
		bBlast = false;
		AAerondelPlayerState* PS = GetPS();
		if (bPos && FVector2D::Distance(P, BlastAt) < 3.5 && PS && PS->Health && !PS->Health->IsDead())
		{
			const float Dr = FMath::Min(0.6f, SumDr(nullptr));
			const int32 X = FMath::RoundToInt(PS->Health->GetMax() * (Run.bHard ? 0.35f : 0.22f) * (1.f - Dr));
			Say(FString::Printf(TEXT("The blast hits you for %d!"), X));
			PS->Health->ApplyDamage(X, B);
		}
		else Say(TEXT("You slip the blast."));
	}
	if (!bBlast && TickN >= VigilNext && bPos)
	{
		bBlast = true;
		BlastAt = P;
		BlastTick = TickN + 3;
		VigilNext = TickN + (Run.Phase == 3 ? 6 : 9);
		Say(TEXT("A red ring forms under you - move!"));
	}
#if ENABLE_DRAW_DEBUG
	if (bBlast)
		if (UWorld* W = GetWorld())
		{
			const FVector C(BlastAt.X * 100.0, BlastAt.Y * 100.0, (B->GetActorLocation().Z - 80.0));
			DrawDebugCircle(W, C, 350.f, 32, FColor(255, 58, 42), false, 0.7f, 0, 8.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
		}
#endif
}

void UVenturesFeature::VigilFinish(bool bWin)
{
	bBlast = false;
	if (!bWin) { Say(TEXT("The Vigil holds. Try again.")); return; }
	const FString Key = Run.Mode + (Run.bHard ? TEXT(":hard") : TEXT(":normal"));
	VigilClears.FindOrAdd(Run.Mode)++;
	const int32 W = VigilWeek();
	if (const int32* L = VigilLock.Find(Key)) if (*L == W) { GiveCoins(500); Say(TEXT("Vigil cleared! Weekly loot already claimed for this tier: +500 coins.")); return; }
	VigilLock.Add(Key, W);
	const int32 CoinsWon = FMath::RoundToInt((1500 + Run.L * 20) * (Run.bHard ? 2.5f : 1.f));
	GiveCoins(CoinsWon);
	GiveXp(StyleSkill(Style()), Run.L * 25.f);
	GiveXp(TEXT("Hitpoints"), Run.L * 8.f);
	Say(FString::Printf(TEXT("Vigil cleared! +%d coins."), CoinsWon));
	BonusDrop(nullptr, TEXT("the Vigil"));
	if (Run.bHard) { BonusDrop(nullptr, TEXT("the Vigil")); GiveItem(TEXT("Greater brutal graft"), 1); }
	Pinnacle(TEXT("Vigil Gate"));
}

void UVenturesFeature::TabVigil(FGPanelModel& M)
{
	M.Title = TEXT("Vigil Gates");
	if (InRunKind(TEXT("vigil")))
	{
		const FGate* G = FindGate(Run.Mode);
		const AAerondelMob* B = Run.Boss.Get();
		M.Lines.Add(FString::Printf(TEXT("%s - phase %d/3"), G ? G->Name : TEXT("Vigil"), Run.Phase));
		if (B) M.Lines.Add(FString::Printf(TEXT("Boss: %d%%"), FMath::Max(0, FMath::RoundToInt(B->GetHp() * 100.f / FMath::Max(1, B->GetMaxHp())))));
		VAct5(M, TEXT("Abandon"), [this] { RunEnd(false); });
		return;
	}
	M.Lines.Add(TEXT("Three-phase boss trials: guards join at 66%, rings come faster below 33%. Step out of red rings. Full loot once per tier each week (Monday 00:00 UTC), with a Pinnacle chest for Lumen."));
	const int32 W = VigilWeek();
	for (const FGate& G : Gates)
	{
		const int32* Cl = VigilClears.Find(G.Key);
		M.Lines.Add(FString::Printf(TEXT("%s - needs level %d - cleared %d"), G.Name, G.Min, Cl ? *Cl : 0));
		const FString K = G.Key;
		for (int32 h = 0; h < 2; ++h)
		{
			const int32* L = VigilLock.Find(K + (h ? TEXT(":hard") : TEXT(":normal")));
			const bool bLooted = L && *L == W;
			VAct5(M, FString::Printf(TEXT("%s %s%s"), G.Name, h ? TEXT("Hard") : TEXT("Normal"), bLooted ? TEXT(" (looted this week)") : TEXT("")),
				[this, K, h] { VigilStart(K, h == 1); }, VLevel() >= G.Min, true);
		}
	}
	M.Lines.Add(FString::Printf(TEXT("Your level %d. Foes scale to you."), VLevel()));
}

// ---------------------------------------------------------------- Elemental Attunements

bool UVenturesFeature::AttuneGearMatch() const
{
	const FElem* E = FindElem(AttuneEl);
	const AAerondelPlayerState* PS = GetPS();
	if (!E || !PS || !PS->Equipment) return false;
	for (const auto& Eq : PS->Equipment->GetAll())
	{
		const UItemDef* I = Eq.Value.Get();
		if (!I) continue;
		const FString N = I->DisplayName.ToString().ToLower();
		for (const TCHAR* W : E->Gear) if (N.Contains(W)) return true;
	}
	return false;
}

float UVenturesFeature::DmgRaid(const UMobDef* Target) const
{
	if (AttuneEl.IsEmpty()) return 0.f;
	float T = AttuneGearMatch() ? 0.05f : 0.f;
	if (AttuneEl == TEXT("ember")) T += 0.05f;
	return T;
}

float UVenturesFeature::DmgRaidActor(const AAerondelMob* Mob) const
{
	if (!Mob) return 0.f;
	if (AttuneEl == TEXT("ember") && HasFrag(TEXT("searing")) && IsBossy(Mob)) return 0.06f;
	if (AttuneEl == TEXT("rime") && HasFrag(TEXT("shatter")) && Mob->IsStunned()) return 0.15f;
	return 0.f;
}

float UVenturesFeature::AccRaid() const { return AttuneEl == TEXT("storm") ? 0.05f + (HasFrag(TEXT("static")) ? 0.05f : 0.f) : 0.f; }
float UVenturesFeature::DrRaid() const { return AttuneEl == TEXT("rime") ? 0.05f + (HasFrag(TEXT("glacial")) ? 0.04f : 0.f) : 0.f; }
float UVenturesFeature::XpRaid(FName SkillId) const { return AttuneEl == TEXT("void") ? 0.04f + (HasFrag(TEXT("hollow")) ? 0.06f : 0.f) : 0.f; }

int32 UVenturesFeature::HitRaid(AAerondelMob* Mob, int32 D)
{
	if (!Mob || D <= 0 || bArcing) return D;
	if (AttuneEl == TEXT("rime") && HasFrag(TEXT("frostbite")) && !IsBossy(Mob) && FMath::FRand() < 0.1f) Mob->Stun(2);
	if (AttuneEl == TEXT("storm") && HasFrag(TEXT("arc")) && FMath::FRand() < 0.15f)
		for (AAerondelMob* O : MobsNear(MobWeb(Mob), 6.f, 3))
		{
			if (O == Mob) continue;
			bArcing = true;
			Strike(O, 0.f, -0.3f, TEXT("arc"));
			bArcing = false;
			break;
		}
	return D;
}

void UVenturesFeature::DiedRaid(AAerondelMob* Mob, bool bByPlayer)
{
	if (!bByPlayer || AttuneEl.IsEmpty()) return;
	AAerondelPlayerState* PS = GetPS();
	if (AttuneEl == TEXT("ember") && HasFrag(TEXT("kindling")) && PS && PS->Health && !PS->Health->IsDead()) PS->Health->Heal(FMath::CeilToInt(PS->Health->GetMax() * 0.02f));
	if (AttuneEl == TEXT("void") && HasFrag(TEXT("orb"))) FlareAdd(5.f);
	SuperReady -= 3000;
}

bool UVenturesFeature::SuperUse()
{
	const FElem* E = FindElem(AttuneEl);
	if (!E) { Say(TEXT("Choose an attunement first (Ventures -> Attune).")); return false; }
	const int64 Now = NowMs();
	if (Now < SuperReady) { Say(FString::Printf(TEXT("%s recharging (%llds)."), E->Super, (long long)((SuperReady - Now + 999) / 1000))); return false; }
	FVector2D P;
	if (!PlayerWebPos(P)) return false;
	TArray<AAerondelMob*> Foes = MobsNear(P, 7.f, 6);
	AAerondelMob* T = CurrentTarget();
	if (T && !T->IsDead() && !Foes.Contains(T)) Foes.Insert(T, 0);
	if (!Foes.Num() && AttuneEl != TEXT("void")) { Say(TEXT("No foes close enough.")); return false; }
	SuperReady = Now + SuperCdMs;
	AttuneSupers++;
	Say(FString::Printf(TEXT("%s!"), E->Super));
	if (AttuneEl == TEXT("ember"))
	{
		const int32 N = HasFrag(TEXT("blaze")) ? 5 : 3;
		for (int32 h = 0; h < N; ++h) for (AAerondelMob* M : Foes) if (M && !M->IsDead()) Strike(M, 0.3f, 0.3f, TEXT("super"));
	}
	if (AttuneEl == TEXT("rime"))
		for (AAerondelMob* M : Foes) { if (!IsBossy(M)) M->Stun(5); Strike(M, 0.4f, 0.5f, TEXT("super")); }
	if (AttuneEl == TEXT("storm"))
	{
		TArray<AAerondelMob*> All = Foes;
		if (HasFrag(TEXT("surge")))
		{
			int32 More = 0;
			for (AAerondelMob* M : MobsNear(P, 12.f, 9)) if (!All.Contains(M) && More < 3) { All.Add(M); ++More; }
		}
		for (AAerondelMob* M : All) if (M && !M->IsDead()) Strike(M, 0.5f, 0.6f, TEXT("super"));
	}
	if (AttuneEl == TEXT("void"))
	{
		for (AAerondelMob* M : Foes) if (M && !M->IsDead()) Strike(M, 0.4f, 0.4f, TEXT("super"));
		if (AAerondelPlayerState* PS = GetPS()) if (PS->Health) PS->Health->Heal(FMath::CeilToInt(PS->Health->GetMax() * (HasFrag(TEXT("well")) ? 0.25f : 0.12f)));
	}
	return true;
}

void UVenturesFeature::AttuneSet(const FString& Element)
{
	const FElem* E = FindElem(Element);
	if (!E) return;
	FVector2D P;
	if (const AAerondelMob* T = CurrentTarget())
		if (!T->IsDead() && PlayerWebPos(P) && FVector2D::Distance(P, MobWeb(T)) < 10.0) { Say(TEXT("Not in the middle of a fight.")); return; }
	AttuneEl = Element;
	AttuneFrags = { FString(E->Frags[0].Key), FString(E->Frags[1].Key) };
}

void UVenturesFeature::FragToggle(const FString& Frag)
{
	const FElem* E = FindElem(AttuneEl);
	if (!E || !HasFragDef(*E, Frag)) return;
	if (AttuneFrags.Contains(Frag)) AttuneFrags.Remove(Frag);
	else if (AttuneFrags.Num() < 2) AttuneFrags.Add(Frag);
	else Say(TEXT("Two fragments at a time. Remove one first."));
}

void UVenturesFeature::TabAttune(FGPanelModel& M)
{
	M.Title = TEXT("Elemental Attunements");
	M.Lines.Add(TEXT("Pick an element: a super on N (90 s cooldown, kills shorten it), a passive class move, and 2 of 3 fragments. Matching gear adds +5% damage."));
	for (const FElem& E : Elems)
	{
		const FString K = FString(*E.Key);
		VAct5(M, FString::Printf(TEXT("%s%s"), AttuneEl == K ? TEXT("[on] ") : TEXT(""), E.Name), [this, K] { AttuneSet(K); });
	}
	const FElem* E = FindElem(AttuneEl);
	if (!E) { M.Lines.Add(TEXT("Choose an element.")); return; }
	const int64 Now = NowMs();
	M.Lines.Add(FString::Printf(TEXT("%s - Super: %s (N) %s"), E->Name, E->Super, Now < SuperReady ? *FString::Printf(TEXT("- %llds"), (long long)((SuperReady - Now + 999) / 1000)) : TEXT("- ready")));
	M.Lines.Add(FString::Printf(TEXT("Class move: %s%s"), E->Move, AttuneGearMatch() ? TEXT(" | Matching gear: +5% damage") : TEXT("")));
	for (const FFrag& F : E->Frags)
	{
		const FString FK = F.Key;
		const bool bOn = AttuneFrags.Contains(FK);
		M.Lines.Add(FString::Printf(TEXT("%s %s - %s"), bOn ? TEXT("[on]") : TEXT("[off]"), F.Name, F.Desc));
		VAct5(M, FString::Printf(TEXT("%s %s"), bOn ? TEXT("Remove") : TEXT("Add"), F.Name), [this, FK] { FragToggle(FK); });
	}
	VAct5(M, FString::Printf(TEXT("Use %s now"), E->Super), [this] { SuperUse(); }, Now >= SuperReady, true);
	M.Lines.Add(FString::Printf(TEXT("Supers used: %d"), AttuneSupers));
}

// ---------------------------------------------------------------- Hearthhold

bool UVenturesFeature::HoldHome() const
{
	FVector2D P;
	if (Run.bOn || !bHoldOwned || !PlayerWebPos(P)) return false;
	const FVector2D C = ZoneC(TEXT("hold"));
	return FMath::Abs(P.X - C.X) < 20.0 && FMath::Abs(P.Y - C.Y) < 16.0;
}

void UVenturesFeature::HoldGo()
{
	if (!bHoldOwned) { Say(TEXT("Buy the island deed first (Ventures -> Hold, 10,000 coins).")); return; }
	if (Run.bOn || Puz.bOn) { Say(TEXT("Finish what you are doing first.")); return; }
	FVector2D P;
	if (PlayerWebPos(P) && P.X > -700.0) HoldReturn = P;
	const FVector2D C = ZoneC(TEXT("hold"));
	TeleportPlayer(FVector2D(C.X, C.Y + 6.0));
	Say(TEXT("You sail to Hearthhold."));
}

void UVenturesFeature::HoldLeave()
{
	const FVector2D B = HoldReturn;
	HoldReturn = FVector2D(0.0, 10.0);
	TeleportPlayer(B);
}

void UVenturesFeature::HoldBuy()
{
	if (bHoldOwned) return;
	if (!TakeCoins(10000)) { Say(TEXT("The deed costs 10,000 coins.")); return; }
	bHoldOwned = true;
	Say(TEXT("Hearthhold is yours!"));
}

void UVenturesFeature::HoldUp(const FString& Building)
{
	const FBuilding* B = FindBuilding(Building);
	if (!B) return;
	int32& L = HoldLv.FindOrAdd(Building);
	if (L >= 3) return;
	const FCost& C = B->Cost[L];
	if (Coins() < C.Gold || CountItem(C.I1) < C.N1 || (C.I2 && CountItem(C.I2) < C.N2)) { Say(FString::Printf(TEXT("Upgrade needs %s in your pack."), *CostText(C))); return; }
	TakeCoins(C.Gold);
	TakeItem(C.I1, C.N1);
	if (C.I2) TakeItem(C.I2, C.N2);
	++L;
	GiveXp(TEXT("Crafting"), 500.f * L);
	Say(FString::Printf(TEXT("%s upgraded to level %d."), B->Name, L));
}

void UVenturesFeature::HoldForge()
{
	const int32* L = HoldLv.Find(TEXT("forge"));
	if (!L || !*L || HoldForgeDay == HoldToday()) return;
	HoldForgeDay = HoldToday();
	GiveItem(*L >= 3 ? TEXT("Greater brutal graft") : TEXT("Lesser keen graft"), 1);
	if (*L >= 2) GiveItem(TEXT("Sigil tome"), 1);
	Say(TEXT("You collect today's forge work."));
}

void UVenturesFeature::HoldGarden()
{
	const int32* L = HoldLv.Find(TEXT("garden"));
	if (!L || !*L) return;
	const double Now = (double)NowMs();
	if (Now - HoldGardenAt < 2.0 * 3600000.0) return;
	HoldGardenAt = Now;
	static const int32 Ns[] = { 0, 6, 12, 20 };
	const int32 N = Ns[FMath::Clamp(*L, 0, 3)];
	for (int32 i = 0; i < N; ++i) GiveItem(Herbs[FMath::RandRange(0, (int32)UE_ARRAY_COUNT(Herbs) - 1)], 1);
	Say(FString::Printf(TEXT("You harvest %d herbs."), N));
}

void UVenturesFeature::HoldSend(FName Companion, const FString& Trip)
{
	const FTrip* T = FindTrip(Trip);
	UCompanionsFeature* Cf = Feature<UCompanionsFeature>();
	if (!T || !Cf || HoldTrips.Contains(Companion) || !Cf->GetState(Companion).bRecruited) return;
	if (Cf->InParty(Companion)) { Say(TEXT("Dismiss them from your side first.")); return; }
	FHoldTrip H;
	H.Kind = Trip;
	H.Back = NowMs() + (int64)T->Mins * 60000;
	HoldTrips.Add(Companion, H);
	const FCompanionDef* D = UCompanionsFeature::Def(Companion);
	Say(FString::Printf(TEXT("%s sets out: %s."), D ? *D->Name : TEXT("Your companion"), T->Name));
}

void UVenturesFeature::HoldCollect(FName Companion)
{
	const FHoldTrip* H = HoldTrips.Find(Companion);
	if (!H || NowMs() < H->Back) return;
	const FTrip* T = FindTrip(H->Kind);
	const bool bLong = H->Kind == TEXT("long");
	HoldTrips.Remove(Companion);
	if (!T) return;
	static const float Mul[] = { 1.f, 1.5f, 2.f, 3.f };
	const int32* KL = HoldLv.Find(TEXT("kennel"));
	const float Mx = Mul[KL ? FMath::Clamp(*KL, 0, 3) : 0];
	TArray<FString> Got;
	for (int32 i = 0; i < T->Rolls; ++i)
	{
		const FLoot& L = HoldLoot[FMath::RandRange(0, (int32)UE_ARRAY_COUNT(HoldLoot) - 1)];
		const int32 Q = FMath::RoundToInt(L.N * Mx);
		GiveItem(L.Item, Q);
		Got.Add(FString::Printf(TEXT("%d %s"), Q, L.Item));
	}
	if (bLong && FMath::FRand() < 0.3f) BonusDrop(nullptr, TEXT("the expedition"));
	HoldDone++;
	Say(FString::Printf(TEXT("Expedition back: %s."), *FString::Join(Got, TEXT(", "))));
}

void UVenturesFeature::TabHold(FGPanelModel& M)
{
	M.Title = TEXT("Hearthhold");
	if (!bHoldOwned)
	{
		M.Lines.Add(TEXT("A private island off the west coast. Upgrade a Forge (daily grafts and tomes), Garden (herbs every 2 h) and Kennel (bigger expedition hauls)."));
		VAct5(M, TEXT("Buy the deed (10,000 coins)"), [this] { HoldBuy(); }, Coins() >= 10000);
		return;
	}
	if (HoldHome()) VAct5(M, TEXT("Sail home"), [this] { HoldLeave(); }, true, true);
	else VAct5(M, TEXT("Sail to Hearthhold"), [this] { HoldGo(); }, !Run.bOn, true);
	const int32* Fl = HoldLv.Find(TEXT("forge"));
	if (Fl && *Fl && HoldForgeDay != HoldToday()) VAct5(M, TEXT("Collect forge"), [this] { HoldForge(); });
	const int32* Gl = HoldLv.Find(TEXT("garden"));
	const double Now = (double)NowMs();
	if (Gl && *Gl && Now - HoldGardenAt >= 7200000.0) VAct5(M, TEXT("Harvest garden"), [this] { HoldGarden(); });
	for (const FBuilding& B : Buildings)
	{
		const int32* Lp = HoldLv.Find(B.Key);
		const int32 L = Lp ? FMath::Clamp(*Lp, 0, 3) : 0;
		const bool bHerbs = FCString::Strcmp(B.Key, TEXT("garden")) == 0;
		const bool bKennel = FCString::Strcmp(B.Key, TEXT("kennel")) == 0;
		M.Lines.Add(FString::Printf(TEXT("%s level %d/3 - %s%s"), B.Name, L, B.Desc[L], bHerbs ? TEXT(" herbs every 2 hours") : bKennel ? TEXT(" expedition hauls") : TEXT(" each day")));
		if (L < 3)
		{
			const FString K = B.Key;
			VAct5(M, FString::Printf(TEXT("Upgrade %s: %s"), B.Name, *CostText(B.Cost[L])), [this, K] { HoldUp(K); });
		}
	}
	M.Lines.Add(FString::Printf(TEXT("Expeditions (%d done):"), HoldDone));
	UCompanionsFeature* Cf = Feature<UCompanionsFeature>();
	int32 Crew = 0;
	if (Cf)
		for (const FCompanionDef& D : UCompanionsFeature::Defs())
		{
			if (!Cf->GetState(D.Id).bRecruited) continue;
			++Crew;
			const FName Id = D.Id;
			if (const FHoldTrip* T = HoldTrips.Find(Id))
			{
				const FTrip* Tr = FindTrip(T->Kind);
				const int64 N = NowMs();
				if (N >= T->Back) { M.Lines.Add(FString::Printf(TEXT("%s - %s: back!"), *D.Name, Tr ? Tr->Name : TEXT(""))); VAct5(M, FString::Printf(TEXT("Collect %s's haul"), *D.Name), [this, Id] { HoldCollect(Id); }); }
				else M.Lines.Add(FString::Printf(TEXT("%s - %s: %lld min"), *D.Name, Tr ? Tr->Name : TEXT(""), (long long)((T->Back - N + 59999) / 60000)));
			}
			else
				for (const FTrip& Tr : Trips)
				{
					const FString TK = Tr.Key;
					VAct5(M, FString::Printf(TEXT("Send %s: %s (%dm)"), *D.Name, Tr.Name, Tr.Mins), [this, Id, TK] { HoldSend(Id, TK); });
				}
		}
	if (!Crew) M.Lines.Add(TEXT("Recruit companions first (Bonds)."));
}
