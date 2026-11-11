#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/AerondelMob.h"
#include "Combat/HealthComponent.h"
#include "World/AerondelScalability.h"
#include "GameFramework/Pawn.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 17 (web adapt/hordewake.js): a short, phone-friendly horde arena.
// You move; your weapons fire on their own. Pick a reward as you level. Not a Proving Grounds duel and not a Burst Tide.

namespace VenturesHorde
{
	static const TCHAR* Foes[] = { TEXT("Goblin"), TEXT("Wolf"), TEXT("Ashen raider"), TEXT("Sand Reaver"), TEXT("Ember Imp"), TEXT("Magma Brute") };
	static constexpr int64 GoalMs = 8 * 60 * 1000;
	struct FWeap { const TCHAR* Id; const TCHAR* Name; const TCHAR* Desc; int32 Ms; bool bCombo = false; };
	static const FWeap Weaps[] = {
		{ TEXT("wand"),       TEXT("Ash Wand"),    TEXT("A slow pulse at the nearest foe."), 700, false },
		{ TEXT("blade"),      TEXT("Orbit Blade"), TEXT("Blades cut anyone who steps close."), 500, false },
		{ TEXT("bow"),        TEXT("Quick Bow"),   TEXT("Shoots where you face."), 550, false },
		{ TEXT("stormrod"),   TEXT("Storm Rod"),   TEXT("A pulse that arcs to a second foe."), 650, true },
		{ TEXT("orbitflare"), TEXT("Orbit Flare"), TEXT("Orbiting blades that also pulse outward."), 480, true },
		{ TEXT("seeker"),     TEXT("Seeker Fan"),  TEXT("Shots fan toward the nearest pack."), 520, true },
	};
	static const FWeap* FindWeap(const FString& Id) { for (const FWeap& W : Weaps) if (Id == W.Id) return &W; return nullptr; }
	static const TCHAR* UnlockText(const FString& Id)
	{
		if (Id == TEXT("blade")) return TEXT("Start with an Orbit Blade");
		if (Id == TEXT("slot")) return TEXT("A third weapon slot");
		if (Id == TEXT("bow")) return TEXT("Start with a Quick Bow");
		return TEXT("");
	}
	static bool IsUnlock(const FString& Id) { return Id == TEXT("blade") || Id == TEXT("slot") || Id == TEXT("bow"); }
	static void VAct17(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesHorde;

void UVenturesFeature::ResetHorde()
{
	HordeBestTime = 0;
	HordeBestLevel = 0;
	HordeRuns = 0;
	HordeUnlocks.Reset();
}

void UVenturesFeature::SaveHorde(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetNumberField(TEXT("bestTime"), (double)HordeBestTime);
	S->SetNumberField(TEXT("bestLevel"), HordeBestLevel);
	S->SetNumberField(TEXT("runs"), HordeRuns);
	TArray<TSharedPtr<FJsonValue>> U;
	for (const FString& Id : HordeUnlocks) U.Add(MakeShared<FJsonValueString>(Id));
	S->SetArrayField(TEXT("unlocks"), U);
	A.SetObjectField(TEXT("horde"), S);
}

void UVenturesFeature::LoadHorde(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("horde"));
	if (!S) return;
	HordeBestTime = FMath::Max<int64>(0, JInt(*S, TEXT("bestTime")));
	HordeBestLevel = FMath::Max(0, (int32)JInt(*S, TEXT("bestLevel")));
	HordeRuns = FMath::Max(0, (int32)JInt(*S, TEXT("runs")));
	if (const TArray<TSharedPtr<FJsonValue>>* U = nullptr; S->TryGetArrayField(TEXT("unlocks"), U) && U)
		for (const TSharedPtr<FJsonValue>& V : *U)
			if (V.IsValid() && V->Type == EJson::String && IsUnlock(V->AsString())) HordeUnlocks.AddUnique(V->AsString());
}

int32 UVenturesFeature::HordeCap() const
{
	const EAerondelTier T = UAerondelScalability::GetAppliedTier();
	return (T == EAerondelTier::Low || T == EAerondelTier::Quest) ? 20 : 35;
}

void UVenturesFeature::HordeSpawn(int32 N)
{
	const int32 Room = HordeCap() - ZoneAlive().Num();
	const int32 Idx = FMath::Clamp((Run.L + Run.Level * 8) / 80, 0, (int32)UE_ARRAY_COUNT(Foes) - 1);
	const FString Type = Foes[Idx];
	for (int32 i = 0; i < FMath::Min(N, Room); ++i)
	{
		const float A = FMath::FRand() * 2.f * PI;
		const float Rad = 8.f + FMath::FRand() * 4.f;
		ZoneSpawn(TEXT("horde"), Type, FMath::Cos(A) * Rad, FMath::Sin(A) * Rad - 2.f, (float)FMath::Max(2, Run.L + Run.Level), 0.55f + Run.Level * 0.03f);
	}
}

AAerondelMob* UVenturesFeature::HordeNearest(float R) const
{
	FVector2D P; if (!PlayerWebPos(P)) return nullptr;
	AAerondelMob* Best = nullptr; float Bd = R;
	for (AAerondelMob* M : ZoneAlive())
	{
		const FVector L = M->GetActorLocation();
		const float D = FVector2D::Distance(P, FVector2D(L.X / 100.0, L.Y / 100.0));
		if (D < Bd) { Bd = D; Best = M; }
	}
	return Best;
}

AAerondelMob* UVenturesFeature::HordeFacing() const
{
	AAerondelPlayerState* PS = GetPS();
	APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	FVector2D P; if (!PlayerWebPos(P) || !Pawn) return HordeNearest(12.f);
	const FVector Fwd = Pawn->GetActorForwardVector();
	AAerondelMob* Best = nullptr; float Bd = 12.f;
	for (AAerondelMob* M : ZoneAlive())
	{
		const FVector L = M->GetActorLocation();
		const FVector2D D(L.X / 100.0 - P.X, L.Y / 100.0 - P.Y);
		const float Len = D.Size();
		if (Len < 0.01f || Len >= Bd) continue;
		if ((D.X * Fwd.X + D.Y * Fwd.Y) / Len > 0.2f) { Bd = Len; Best = M; }
	}
	return Best ? Best : HordeNearest(12.f);
}

void UVenturesFeature::HordeStrike(AAerondelMob* M, float Dmg)
{
	if (M && !M->IsDead() && M->GetHp() > 0) Strike(M, 0.25f, Dmg, TEXT("horde"));
}

void UVenturesFeature::HordeFire(const FString& Id)
{
	const FWeap* W = FindWeap(Id); if (!W) return;
	const int64 Now = NowMs();
	const float Haste = 1.f - FMath::Min(0.35f, Run.Haste * 0.08f);
	const int64* Last = Run.ShotAt.Find(Id);
	if (Last && Now - *Last < (int64)(W->Ms * Haste)) return;
	Run.ShotAt.Add(Id, Now);
	FVector2D P; PlayerWebPos(P);
	const float Reach = 3.2f + Run.Mag * 0.4f;
	if (Id == TEXT("blade") || Id == TEXT("orbitflare"))
	{
		for (AAerondelMob* M : ZoneAlive())
		{
			const FVector L = M->GetActorLocation();
			const float D = FVector2D::Distance(P, FVector2D(L.X / 100.0, L.Y / 100.0));
			if (D < (Id == TEXT("orbitflare") ? 5.f + Run.Mag * 0.3f : Reach)) HordeStrike(M, -0.15f);
		}
		if (Id == TEXT("orbitflare")) if (AAerondelMob* M = HordeNearest(10.f)) HordeStrike(M, -0.25f);
	}
	else if (Id == TEXT("bow") || Id == TEXT("seeker"))
	{
		AAerondelMob* M = HordeFacing();
		if (M) HordeStrike(M, -0.15f);
		if (Id == TEXT("seeker"))
		{
			int32 C = 0;
			for (AAerondelMob* Q : ZoneAlive())
			{
				if (Q == M || C >= 2) continue;
				const FVector L = Q->GetActorLocation();
				if (FVector2D::Distance(P, FVector2D(L.X / 100.0, L.Y / 100.0)) < 8.f) { HordeStrike(Q, -0.3f); ++C; }
			}
		}
	}
	else
	{
		AAerondelMob* M = HordeNearest(11.f);
		if (M) HordeStrike(M, -0.15f);
		if (Id == TEXT("stormrod"))
			for (AAerondelMob* Q : ZoneAlive()) if (Q != M) { HordeStrike(Q, -0.3f); break; }
	}
}

void UVenturesFeature::HordeDeck()
{
	TArray<FRunPick> Opts;
	TSet<FString> Have;
	for (const FString& Id : Run.Weapons) Have.Add(Id);
	for (const TCHAR* Id : { TEXT("wand"), TEXT("blade"), TEXT("bow") })
		if (!Have.Contains(Id) && Run.Weapons.Num() < Run.Slots)
			if (const FWeap* W = FindWeap(Id)) { FRunPick P; P.T = TEXT("wep"); P.Id = Id; P.Name = W->Name; P.Desc = TEXT("Add this weapon. It fires on its own."); Opts.Add(P); }
	for (const FString& Id : Run.Weapons)
		if (const FWeap* W = FindWeap(Id)) if (!W->bCombo)
		{ FRunPick P; P.T = TEXT("haste"); P.Id = Id; P.Name = FString::Printf(TEXT("Tune %s"), W->Name); P.Desc = TEXT("It fires a little more often."); Opts.Add(P); }
	auto Combo = [&](const TCHAR* A, const TCHAR* B, const TCHAR* Out)
	{
		if (Have.Contains(A) && Have.Contains(B) && !Have.Contains(Out))
			if (const FWeap* W = FindWeap(Out)) { FRunPick P; P.T = TEXT("combo"); P.Id = Out; P.From = FString::Printf(TEXT("%s+%s"), A, B); P.Name = FString::Printf(TEXT("Combine into %s"), W->Name); P.Desc = W->Desc; Opts.Add(P); }
	};
	Combo(TEXT("blade"), TEXT("wand"), TEXT("orbitflare"));
	Combo(TEXT("wand"), TEXT("bow"), TEXT("stormrod"));
	Combo(TEXT("blade"), TEXT("bow"), TEXT("seeker"));
	{ FRunPick P; P.T = TEXT("mag"); P.Name = TEXT("Wide Wake"); P.Desc = TEXT("Pickups and orbit reach a little farther."); Opts.Add(P); }
	{ FRunPick P; P.T = TEXT("heart"); P.Name = TEXT("Hearth sip"); P.Desc = TEXT("Restore a little health (not a damage bonus)."); Opts.Add(P); }
	Run.Picks.Reset();
	while (Run.Picks.Num() < 3 && Opts.Num())
	{
		const int32 I = FMath::RandRange(0, Opts.Num() - 1);
		Run.Picks.Add(Opts[I]);
		Opts.RemoveAt(I);
	}
}

bool UVenturesFeature::HordeStart()
{
	if (!RunStart(TEXT("horde"), TEXT("horde"))) return false;
	Run.L = VLevel();
	Run.Level = 1;
	Run.Slots = 2 + (HordeUnlocks.Contains(TEXT("slot")) ? 1 : 0);
	Run.Weapons.Add(TEXT("wand"));
	if (HordeUnlocks.Contains(TEXT("blade"))) Run.Weapons.Add(TEXT("blade"));
	if (HordeUnlocks.Contains(TEXT("bow")) && Run.Weapons.Num() < Run.Slots) Run.Weapons.Add(TEXT("bow"));
	while (Run.Weapons.Num() > Run.Slots) Run.Weapons.Pop();
	Run.GoalMs = GoalMs;
	HordeSpawn(6);
	Say(TEXT("Hordewake begins. Move — your weapons fire on their own. Survive the timer."));
	return true;
}

bool UVenturesFeature::HordePick(int32 I)
{
	if (!InRunKind(TEXT("horde")) || !Run.Picks.IsValidIndex(I)) return false;
	const FRunPick C = Run.Picks[I];
	if (C.T == TEXT("wep") && Run.Weapons.Num() < Run.Slots && !Run.Weapons.Contains(C.Id)) Run.Weapons.Add(C.Id);
	if (C.T == TEXT("haste")) Run.Haste++;
	if (C.T == TEXT("combo"))
	{
		FString A, B; C.From.Split(TEXT("+"), &A, &B);
		Run.Weapons.RemoveAll([&](const FString& W) { return W == A || W == B; });
		if (!Run.Weapons.Contains(C.Id)) Run.Weapons.Add(C.Id);
	}
	if (C.T == TEXT("mag")) Run.Mag++;
	if (C.T == TEXT("heart"))
	{
		if (AAerondelPlayerState* PS = GetPS()) if (PS->Health && PS->Health->GetCurrent() > 0)
			PS->Health->Heal(FMath::Max(1, FMath::RoundToInt(PS->Health->GetMax() * 0.15f)));
	}
	Run.Picks.Reset();
	Run.bPaused = false;
	Say(FString::Printf(TEXT("You take %s."), *C.Name));
	return true;
}

void UVenturesFeature::TickHordeFire()
{
	if (!InRunKind(TEXT("horde")) || Run.bPaused) return;
	for (const FString& Id : Run.Weapons) HordeFire(Id);
}

void UVenturesFeature::TickHorde()
{
	if (!InRunKind(TEXT("horde")) || Run.bPaused) return;
	if (NowMs() - Run.StartMs >= (Run.GoalMs > 0 ? Run.GoalMs : GoalMs)) { RunEnd(true); return; }
	const int32 Need = FMath::Min(HordeCap(), 6 + Run.Level / 2);
	if (ZoneAlive().Num() < Need) HordeSpawn(4);
}

void UVenturesFeature::DiedHorde(AAerondelMob* Mob, bool bByPlayer)
{
	if (!InRunKind(TEXT("horde")) || !Mob || !bByPlayer || Run.bPaused) return;
	bool bOurs = false;
	for (const TWeakObjectPtr<AAerondelMob>& W : Run.Foes) if (W.Get() == Mob) { bOurs = true; break; }
	if (!bOurs) return;
	Run.Kills++;
	Run.Xp += 8 + Run.Level / 2;
	const int32 Need = 20 + Run.Level * 12;
	if (Run.Xp >= Need)
	{
		Run.Xp -= Need;
		Run.Level++;
		Run.bPaused = true;
		HordeDeck();
		Say(FString::Printf(TEXT("Hordewake level %d. Pick a reward."), Run.Level));
	}
}

void UVenturesFeature::HordeFinish(bool bWin)
{
	HordeRuns++;
	if (bWin)
	{
		const int64 T = NowMs() - Run.StartMs;
		if (T > HordeBestTime) HordeBestTime = T;
		if (Run.Level > HordeBestLevel) HordeBestLevel = Run.Level;
		if (!HordeUnlocks.Contains(TEXT("blade"))) { HordeUnlocks.Add(TEXT("blade")); Say(TEXT("Unlock: you will start the next Hordewake with an Orbit Blade.")); }
		else if (!HordeUnlocks.Contains(TEXT("slot")) && Run.Level >= 6) { HordeUnlocks.Add(TEXT("slot")); Say(TEXT("Unlock: a third weapon slot in Hordewake.")); }
		else if (!HordeUnlocks.Contains(TEXT("bow"))) { HordeUnlocks.Add(TEXT("bow")); Say(TEXT("Unlock: you can start with a Quick Bow.")); }
		const int32 Coins = FMath::RoundToInt(80 + Run.L * 2 + Run.Kills * 3);
		GiveCoins(Coins);
		Say(FString::Printf(TEXT("Hordewake held for %ds (level %d). +%d coins."), FMath::RoundToInt(T / 1000.f), Run.Level, Coins));
	}
	else Say(FString::Printf(TEXT("Hordewake ends at level %d. The overworld is as you left it."), Run.Level));
}

void UVenturesFeature::TabHorde(FGPanelModel& M)
{
	if (InRunKind(TEXT("horde")) && Run.Picks.Num())
	{
		M.Title = FString::Printf(TEXT("Level %d — pick a reward"), Run.Level);
		for (int32 I = 0; I < Run.Picks.Num(); ++I)
		{
			const FRunPick& C = Run.Picks[I];
			M.Lines.Add(FString::Printf(TEXT("%s — %s"), *C.Name, *C.Desc));
			VAct17(M, C.Name, [this, I] { HordePick(I); });
		}
		return;
	}
	if (InRunKind(TEXT("horde")))
	{
		const int64 Left = FMath::Max<int64>(0, (Run.GoalMs > 0 ? Run.GoalMs : GoalMs) - (NowMs() - Run.StartMs));
		M.Title = FString::Printf(TEXT("Hordewake · %llds · lv %d"), (Left + 999) / 1000, Run.Level);
		TArray<FString> Names;
		for (const FString& Id : Run.Weapons) if (const FWeap* W = FindWeap(Id)) Names.Add(W->Name);
		M.Lines.Add(FString::Printf(TEXT("Weapons: %s · kills %d · foes %d/%d"), *FString::Join(Names, TEXT(", ")), Run.Kills, ZoneAlive().Num(), HordeCap()));
		VAct17(M, TEXT("Abandon run"), [this] { RunEnd(false); }, true, true);
		return;
	}
	M.Title = FString::Printf(TEXT("Hordewake · best %s · runs %d"), HordeBestLevel ? *FString::Printf(TEXT("level %d"), HordeBestLevel) : TEXT("—"), HordeRuns);
	M.Lines.Add(TEXT("A short horde arena. You move; your weapons fire on their own. Pick a reward as you level. Some weapons combine into a stronger form. Waves scale with you, but a low-level run stays fair. At most 20 foes on Low. This is not a Proving Grounds duel and not a Burst Tide."));
	TArray<FString> Un;
	for (const FString& Id : HordeUnlocks) Un.Add(UnlockText(Id));
	M.Lines.Add(FString::Printf(TEXT("Unlocks: %s"), Un.Num() ? *FString::Join(Un, TEXT(" · ")) : TEXT("none yet. Survive a run to earn a starting blade.")));
	VAct17(M, TEXT("Start Hordewake"), [this] { HordeStart(); }, !Run.bOn, true);
}
