#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/AerondelMob.h"
#include "Combat/HealthComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 19 (web adapt/ashspire.js): a roguelike tower of rooms, boons from original hearth-spirits,
// and meta-unlocks between runs. Death or leaving ends the climb — there is no mid-run resume. Not the Chronicle.

namespace VenturesSpire
{
	static const TCHAR* Foes[] = { TEXT("Ashen raider"), TEXT("Sand Reaver"), TEXT("Ember Imp"), TEXT("Magma Brute"), TEXT("Canopy Stalker"), TEXT("Rime Wraith"), TEXT("Voidborn Knight") };
	static constexpr int32 Floors = 7;
	struct FBoon { const TCHAR* Id; const TCHAR* Name; const TCHAR* Desc; const TCHAR* Spirit; };
	static const FBoon Boons[] = {
		{ TEXT("sip"),       TEXT("Warm sip"),     TEXT("Restore a little health now."), TEXT("Hearthmother") },
		{ TEXT("aftercare"), TEXT("Aftercare"),    TEXT("Restore a little health whenever a room clears."), TEXT("Hearthmother") },
		{ TEXT("firstbind"), TEXT("First bind"),   TEXT("The first foe you strike in a room is briefly stunned."), TEXT("Ashwarden") },
		{ TEXT("kindsteel"), TEXT("Kind steel"),   TEXT("Your stuns last a tick longer in the Spire."), TEXT("Ashwarden") },
		{ TEXT("echo"),      TEXT("Ley echo"),     TEXT("Your hits sometimes leave a second, lighter pulse."), TEXT("Leywright") },
		{ TEXT("sight"),     TEXT("Ley sight"),    TEXT("Foes in the Spire are a little easier to read (accuracy, not damage)."), TEXT("Leywright") },
		{ TEXT("slowstep"),  TEXT("Slowstep"),     TEXT("A struck foe is briefly slowed."), TEXT("Windcant") },
		{ TEXT("cinder"),    TEXT("Cinder wake"),  TEXT("A fallen foe can singe a neighbour — a light follow-up, not a big hit."), TEXT("Emberkin") },
	};
	struct FUnlock { const TCHAR* Id; const TCHAR* Name; const TCHAR* Desc; int32 Cost; };
	static const FUnlock Unlocks[] = {
		{ TEXT("kindle"), TEXT("Kindle"), TEXT("Start each climb with 2 extra Ash."), 8 },
		{ TEXT("ward"), TEXT("Ashwarden's greeting"), TEXT("Start each climb with First bind already in hand."), 12 },
		{ TEXT("purse"), TEXT("Ember purse"), TEXT("A finished or failed climb yields a little more Ash."), 10 },
	};
	static const FBoon* FindBoon(const FString& Id) { for (const FBoon& B : Boons) if (Id == B.Id) return &B; return nullptr; }
	static const FUnlock* FindUnlock(const FString& Id) { for (const FUnlock& U : Unlocks) if (Id == U.Id) return &U; return nullptr; }
	static FString PhaseOf(int32 F) { return F == Floors ? TEXT("boss") : (F == 3 || F == 6) ? TEXT("event") : TEXT("fight"); }
	static const TCHAR* PickFoe() { return Foes[FMath::RandRange(0, (int32)UE_ARRAY_COUNT(Foes) - 1)]; }
	static void VAct19(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesSpire;

void UVenturesFeature::ResetSpire()
{
	SpireAsh = 0;
	SpireUnlocks.Reset();
	SpireBestFloor = 0;
	SpireRuns = 0;
}

void UVenturesFeature::SaveSpire(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetNumberField(TEXT("currency"), SpireAsh);
	TArray<TSharedPtr<FJsonValue>> U;
	for (const FString& Id : SpireUnlocks) U.Add(MakeShared<FJsonValueString>(Id));
	S->SetArrayField(TEXT("unlocks"), U);
	S->SetNumberField(TEXT("bestFloor"), SpireBestFloor);
	S->SetNumberField(TEXT("runs"), SpireRuns);
	A.SetObjectField(TEXT("spire"), S);
}

void UVenturesFeature::LoadSpire(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("spire"));
	if (!S) return;
	SpireAsh = FMath::Max(0, (int32)JInt(*S, TEXT("currency")));
	SpireBestFloor = FMath::Max(0, (int32)JInt(*S, TEXT("bestFloor")));
	SpireRuns = FMath::Max(0, (int32)JInt(*S, TEXT("runs")));
	if (const TArray<TSharedPtr<FJsonValue>>* U = nullptr; S->TryGetArrayField(TEXT("unlocks"), U) && U)
		for (const TSharedPtr<FJsonValue>& V : *U)
			if (V.IsValid() && V->Type == EJson::String && FindUnlock(V->AsString())) SpireUnlocks.AddUnique(V->AsString());
}

void UVenturesFeature::SpireHeal(float Frac)
{
	if (AAerondelPlayerState* PS = GetPS()) if (PS->Health && PS->Health->GetCurrent() > 0)
		PS->Health->Heal(FMath::Max(1, FMath::RoundToInt(PS->Health->GetMax() * Frac)));
}

bool UVenturesFeature::SpireHas(const FString& Id) const
{
	return InRunKind(TEXT("spire")) && Run.Boons.Contains(Id);
}

void UVenturesFeature::SpireSpawnFloor()
{
	for (AAerondelMob* M : ZoneAlive()) if (M) M->Destroy();
	Run.Foes.Reset();
	Run.bBound = false;
	if (Run.Mode != TEXT("fight") && Run.Mode != TEXT("boss")) return;
	const bool bBoss = Run.Mode == TEXT("boss");
	const int32 N = bBoss ? 1 : 3 + FMath::Min(2, Run.Floor / 2);
	const FString Type = bBoss ? TEXT("Voidborn Knight") : PickFoe();
	int32 L = FMath::RoundToInt(Run.L * (1.f + Run.Floor * 0.08f) + (bBoss ? SpireBestFloor * 4 : 0));
	if (bBoss && LumenOn()) L += FMath::Max(0, FMath::RoundToInt(LumenScore - 1000.f));
	const int32 Room = FMath::Max(1, CapFoes(15) - ZoneAlive().Num());
	for (int32 i = 0; i < FMath::Min(N, Room); ++i)
	{
		const float A = FMath::FRand() * 2.f * PI;
		const float Rad = 4.f + FMath::FRand() * 5.f;
		ZoneSpawn(TEXT("spire"), Type, FMath::Cos(A) * Rad, FMath::Sin(A) * Rad - 3.f, (float)L, bBoss ? 2.2f : 1.f, bBoss && i == 0);
	}
}

void UVenturesFeature::SpireEnterFloor()
{
	Run.Mode = PhaseOf(Run.Floor);
	Run.Picks.Reset();
	Run.bBound = false;
	if (Run.Mode == TEXT("event"))
	{
		FRunPick R; R.T = TEXT("event"); R.Id = TEXT("rest"); R.Name = TEXT("Hearth rest"); R.Desc = TEXT("Sit a moment and restore some health."); Run.Picks.Add(R);
		R.Id = TEXT("gift"); R.Name = TEXT("A spirit's gift"); R.Desc = TEXT("Take a random boon you do not already have."); Run.Picks.Add(R);
		R.Id = TEXT("ash"); R.Name = TEXT("Ash cache"); R.Desc = TEXT("Take 2 Ash now, mid-run."); Run.Picks.Add(R);
	}
	else SpireSpawnFloor();
	Say(Run.Mode == TEXT("boss") ? TEXT("The Ashspire guardian waits.") : Run.Mode == TEXT("event") ? FString::Printf(TEXT("Floor %d: a quiet landing."), Run.Floor) : FString::Printf(TEXT("Floor %d of %d."), Run.Floor, Floors));
}

bool UVenturesFeature::SpireStart()
{
	if (!RunStart(TEXT("spire"), TEXT("spire"))) return false;
	Run.L = VLevel();
	Run.Floor = 1;
	Run.Boons.Reset();
	if (SpireUnlocks.Contains(TEXT("ward"))) Run.Boons.Add(TEXT("firstbind"));
	Run.AshEarned = SpireUnlocks.Contains(TEXT("kindle")) ? 2 : 0;
	SpireEnterFloor();
	Say(TEXT("Ashspire opens. Climb the rooms, take a spirit's boon, and do not expect to resume if you leave."));
	return true;
}

void UVenturesFeature::SpireDeck()
{
	TArray<const FBoon*> Pool;
	for (const FBoon& B : Boons) if (!Run.Boons.Contains(B.Id)) Pool.Add(&B);
	Run.Picks.Reset();
	while (Run.Picks.Num() < 3 && Pool.Num())
	{
		const int32 I = FMath::RandRange(0, Pool.Num() - 1);
		FRunPick P; P.T = TEXT("boon"); P.Id = Pool[I]->Id; P.Name = Pool[I]->Name; P.Desc = Pool[I]->Desc; P.From = Pool[I]->Spirit;
		Run.Picks.Add(P);
		Pool.RemoveAt(I);
	}
}

bool UVenturesFeature::SpirePick(int32 I)
{
	if (!InRunKind(TEXT("spire")) || !Run.Picks.IsValidIndex(I)) return false;
	const FRunPick C = Run.Picks[I];
	if (C.T == TEXT("boon"))
	{
		if (C.Id == TEXT("sip")) SpireHeal(0.2f);
		else if (!Run.Boons.Contains(C.Id)) Run.Boons.Add(C.Id);
		Say(FString::Printf(TEXT("%s grants %s."), *C.From, *C.Name));
	}
	else
	{
		if (C.Id == TEXT("rest")) SpireHeal(0.3f);
		if (C.Id == TEXT("gift"))
		{
			TArray<FString> Pool;
			for (const FBoon& B : Boons) if (!Run.Boons.Contains(B.Id) && B.Id != TEXT("sip")) Pool.Add(B.Id);
			if (Pool.Num()) Run.Boons.Add(Pool[FMath::RandRange(0, Pool.Num() - 1)]);
		}
		if (C.Id == TEXT("ash")) Run.AshEarned += 2;
		Say(FString::Printf(TEXT("You take %s."), *C.Name));
	}
	Run.Picks.Reset();
	Run.Floor++;
	SpireEnterFloor();
	return true;
}

bool UVenturesFeature::SpireBuy(const FString& Id)
{
	const FUnlock* U = FindUnlock(Id); if (!U) return false;
	if (SpireUnlocks.Contains(Id)) { Say(TEXT("You already have that.")); return false; }
	if (SpireAsh < U->Cost) { Say(FString::Printf(TEXT("That costs %d Ash."), U->Cost)); return false; }
	SpireAsh -= U->Cost; SpireUnlocks.Add(Id);
	Say(FString::Printf(TEXT("Unlock: %s."), U->Name));
	return true;
}

void UVenturesFeature::TickSpire()
{
	if (!InRunKind(TEXT("spire")) || Run.Picks.Num()) return;
	if (Run.Mode == TEXT("fight") || Run.Mode == TEXT("boss"))
	{
		if (ZoneAlive().Num() == 0)
		{
			if (SpireHas(TEXT("aftercare"))) SpireHeal(0.12f);
			if (Run.Mode == TEXT("boss")) { RunEnd(true); return; }
			SpireDeck();
			Say(TEXT("The spirits offer a boon."));
		}
	}
}

int32 UVenturesFeature::HitSpire(AAerondelMob* Mob, int32 D)
{
	if (!InRunKind(TEXT("spire")) || !Mob || D <= 0) return D;
	bool bOurs = false;
	for (const TWeakObjectPtr<AAerondelMob>& W : Run.Foes) if (W.Get() == Mob) { bOurs = true; break; }
	if (!bOurs) return D;
	if (SpireHas(TEXT("firstbind")) && !Run.bBound)
	{
		Mob->Stun(3 + (SpireHas(TEXT("kindsteel")) ? 1 : 0));
		Run.bBound = true;
	}
	else if (SpireHas(TEXT("kindsteel")) || SpireHas(TEXT("slowstep")))
		Mob->Stun(1 + (SpireHas(TEXT("kindsteel")) ? 1 : 0));
	if (SpireHas(TEXT("echo")) && FMath::FRand() < 0.2f) Strike(Mob, 0.15f, -0.35f, TEXT("spire-echo"));
	return D;
}

void UVenturesFeature::DiedSpire(AAerondelMob* Mob, bool bByPlayer)
{
	if (!InRunKind(TEXT("spire")) || !Mob || !bByPlayer || !SpireHas(TEXT("cinder"))) return;
	for (AAerondelMob* Q : ZoneAlive()) if (Q != Mob) { Strike(Q, 0.2f, -0.3f, TEXT("spire-cinder")); break; }
}

float UVenturesFeature::AccSpire() const
{
	return SpireHas(TEXT("sight")) ? 0.08f : 0.f;
}

void UVenturesFeature::SpireFinish(bool bWin)
{
	SpireRuns++;
	if (Run.Floor > SpireBestFloor) SpireBestFloor = Run.Floor;
	int32 Ash = FMath::Max(1, Run.Floor) + (bWin ? 5 : 0) + Run.AshEarned;
	if (SpireUnlocks.Contains(TEXT("purse"))) Ash += 2;
	SpireAsh += Ash;
	Say(bWin ? FString::Printf(TEXT("The Ashspire falls. +%d Ash. Best floor %d."), Ash, SpireBestFloor)
	         : FString::Printf(TEXT("You leave the Ashspire at floor %d. +%d Ash. This climb cannot be resumed."), Run.Floor, Ash));
}

void UVenturesFeature::TabSpire(FGPanelModel& M)
{
	if (InRunKind(TEXT("spire")) && Run.Picks.Num())
	{
		M.Title = Run.Mode == TEXT("event") ? FString::Printf(TEXT("Floor %d — a quiet landing"), Run.Floor) : TEXT("The spirits offer a boon");
		for (int32 I = 0; I < Run.Picks.Num(); ++I)
		{
			const FRunPick& C = Run.Picks[I];
			M.Lines.Add(C.From.IsEmpty() ? FString::Printf(TEXT("%s — %s"), *C.Name, *C.Desc) : FString::Printf(TEXT("%s (%s) — %s"), *C.Name, *C.From, *C.Desc));
			VAct19(M, C.Name, [this, I] { SpirePick(I); });
		}
		return;
	}
	if (InRunKind(TEXT("spire")))
	{
		M.Title = FString::Printf(TEXT("Ashspire · floor %d/%d · %s"), Run.Floor, Floors, *Run.Mode);
		if (Run.Boons.Num()) { TArray<FString> N; for (const FString& Id : Run.Boons) if (const FBoon* B = FindBoon(Id)) N.Add(B->Name); M.Lines.Add(TEXT("Boons: ") + FString::Join(N, TEXT(", "))); }
		M.Lines.Add(FString::Printf(TEXT("Foes %d/%d"), ZoneAlive().Num(), CapFoes(15)));
		VAct19(M, TEXT("Leave the climb (it cannot be resumed)"), [this] { RunEnd(false); }, true, true);
		return;
	}
	M.Title = FString::Printf(TEXT("Ashspire · Ash %d · best floor %d · runs %d"), SpireAsh, SpireBestFloor, SpireRuns);
	M.Lines.Add(TEXT("A roguelike tower: fight, take a boon from an original hearth-spirit (Hearthmother, Ashwarden, Leywright, Windcant, Emberkin), sometimes a quiet landing, then a guardian that scales with your best floor and Lumen. Death or leaving grants Ash and ends the climb — there is no mid-run resume. The Chronicle stays a separate system."));
	for (const FUnlock& U : Unlocks)
	{
		const bool bHave = SpireUnlocks.Contains(U.Id);
		M.Lines.Add(FString::Printf(TEXT("%s (%d Ash) — %s%s"), U.Name, U.Cost, U.Desc, bHave ? TEXT(" (owned)") : TEXT("")));
		if (!bHave) VAct19(M, FString::Printf(TEXT("Unlock %s"), U.Name), [this, Id = FString(U.Id)] { SpireBuy(Id); }, SpireAsh >= U.Cost);
	}
	VAct19(M, TEXT("Start the climb"), [this] { SpireStart(); }, !Run.bOn, true);
}
