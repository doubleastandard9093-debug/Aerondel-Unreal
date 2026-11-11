#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Combat/AerondelMob.h"
#include "Combat/HealthComponent.h"
#include "Data/MobDef.h"
#include "Data/ItemDef.h"
#include "Inventory/EquipmentComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ============================================================================================
// Ventures group 8 (web adapt/guardbreak.js): ordinary armed foes have a small poise bar.
// Heavy hits, war fans, glaives, techniques and Tetherline fill it; a break stuns and opens a short riposte.
// Bosses keep Crestbreak as their big stagger. Guardbreak can feed that bar but will not lock a boss.
// ============================================================================================

static void VAct8(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }

static bool GuardArmed(const AAerondelMob* M)
{
	return M && M->Definition && M->Definition->MaxHit > 0 && M->GetMaxHp() > 0;
}

void UVenturesFeature::ResetGuard()
{
	GuardBreaks = 0;
	GuardRipostes = 0;
	GuardPoise.Reset();
	PlayerPoise = -1.f;
	PlayerPoiseCd = 0;
}

void UVenturesFeature::SaveGuard(FJsonObject& A) const
{
	TSharedRef<FJsonObject> G = MakeShared<FJsonObject>();
	G->SetNumberField(TEXT("breaks"), GuardBreaks);
	G->SetNumberField(TEXT("ripostes"), GuardRipostes);
	A.SetObjectField(TEXT("guard"), G);
}

void UVenturesFeature::LoadGuard(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> G = JObj(A, TEXT("guard"));
	if (!G) return;
	GuardBreaks = FMath::Max(0, (int32)JInt(*G, TEXT("breaks")));
	GuardRipostes = FMath::Max(0, (int32)JInt(*G, TEXT("ripostes")));
}

int32 UVenturesFeature::GuardPoiseMax(const AAerondelMob* Mob) const
{
	if (!Mob) return 18;
	const int32 L = Mob->Definition ? Mob->Definition->CombatLevel : 1;
	return FMath::Max(18, FMath::RoundToInt(16 + L * 0.45f + (IsBossy(Mob) ? 20 : 0)));
}

int32 UVenturesFeature::GuardPoiseOf(const AAerondelMob* Mob) const
{
	if (const FGuardPoise* P = GuardPoise.Find(Mob)) return FMath::RoundToInt(P->Poise);
	return 0;
}

bool UVenturesFeature::GuardRiposteOpen(const AAerondelMob* Mob) const
{
	const FGuardPoise* P = GuardPoise.Find(Mob);
	return P && TickNow() < P->RiposteUntil && !P->bUsed;
}

int32 UVenturesFeature::PlayerPoiseMax() const
{
	float N = 14.f + SkillLevel(TEXT("Defence")) * 0.35f;
	const AAerondelPlayerState* PS = GetPS();
	if (PS && PS->Equipment)
	{
		for (EEquipSlot Sl : { EEquipSlot::Body, EEquipSlot::Legs, EEquipSlot::Head, EEquipSlot::Shield })
		{
			const UItemDef* I = PS->Equipment->GetEquipped(Sl);
			const int32 Def = I ? I->Bonuses.Defence : 0;
			if (Def >= 20) N += 4.f + FMath::Min(8.f, Def / 20.f);
		}
	}
	return FMath::RoundToInt(N);
}

int32 UVenturesFeature::HitGuard(AAerondelMob* Mob, int32 D)
{
	if (!Mob || D <= 0 || !GuardArmed(Mob)) return D;
	FGuardPoise& P = GuardPoise.FindOrAdd(Mob);
	const bool bWasOpen = TickNow() < P.RiposteUntil && !P.bUsed;
	if (bWasOpen)
	{
		// Next damaging hit in the window auto-crits once (a window, not a standing damage bonus).
		D *= 2;
		P.bUsed = true;
		P.RiposteUntil = 0;
		GuardRipostes++;
	}
	if (P.CdUntil && TickNow() < P.CdUntil) return D;
	if (TickNow() < P.RiposteUntil) return D;
	const int32 Max = GuardPoiseMax(Mob);
	const FName Cls = WeaponClass();
	float N = 6.f + FMath::Min(14.f, D / FMath::Max(4.f, Mob->GetMaxHp() * 0.08f) * 8.f);
	if (Cls == TEXT("warfan") || Cls == TEXT("glaive")) N *= 1.8f;
	if (StrikeKind && (FCString::Strcmp(StrikeKind, TEXT("flare")) == 0 || FCString::Strcmp(StrikeKind, TEXT("tether")) == 0))
	{
		if (FCString::Strcmp(StrikeKind, TEXT("tether")) == 0) N += 16.f;
		else N *= 2.2f;
	}
	if (IsBossy(Mob)) N *= 0.55f;
	P.Poise = FMath::Min((float)Max, P.Poise + N);
	if (P.Poise + 0.01f < Max) return D;
	const bool bBoss = IsBossy(Mob);
	GuardBreaks++;
	P.Poise = 0.f;
	P.CdUntil = TickNow() + (bBoss ? 10 : 6);
	const int32 Stun = bBoss ? 1 : 2;
	if (!(bBoss && IsBroken(Mob))) Mob->Stun(Stun);
	if (bBoss) Stagger(Mob, 14.f);
	P.RiposteUntil = TickNow() + (bBoss ? 2 : 5);
	P.bUsed = false;
	const FString Name = Mob->GetDisplayName().ToString();
	Say(bBoss ? FString::Printf(TEXT("Guardbreak! The %s staggers (Crestbreak is fed)."), *Name)
	          : FString::Printf(TEXT("Guardbreak! The %s is open to a riposte."), *Name));
	return D;
}

bool UVenturesFeature::GuardRiposte(AAerondelMob* Mob)
{
	if (!Mob) Mob = CurrentTarget();
	if (!Mob || !GuardRiposteOpen(Mob)) { Say(TEXT("There is no opening to riposte.")); return false; }
	FVector2D Pos;
	if (!PlayerWebPos(Pos)) return false;
	const FVector MLoc = Mob->GetActorLocation();
	if (FVector2D::Distance(Pos, FVector2D(MLoc.X / 100.0, MLoc.Y / 100.0)) > 4.f) { Say(TEXT("Step in closer to riposte.")); return false; }
	const int32 D = Strike(Mob, 2.f, 1.f, TEXT("riposte"));
	if (D <= 0 && GuardRiposteOpen(Mob)) { Say(TEXT("The riposte misses.")); return false; }
	return true;
}

void UVenturesFeature::IncomingGuard(int32 Amount)
{
	if (Amount <= 0) return;
	if (VocHoldActive()) { PlayerPoiseCd = 0; return; }
	const int32 Max = PlayerPoiseMax();
	if (PlayerPoise < 0.f) PlayerPoise = (float)Max;
	const AAerondelPlayerState* PS = GetPS();
	const int32 MaxHp = (PS && PS->Health) ? PS->Health->GetMax() : 10;
	const bool bHeavy = Amount >= FMath::Max(7, FMath::RoundToInt(MaxHp * 0.12f));
	if (!bHeavy) return;
	if (PlayerPoiseCd && TickNow() < PlayerPoiseCd) return;
	PlayerPoise -= 28.f + FMath::Min(40.f, Amount * 1.2f);
	if (PlayerPoise > 0.f) return;
	PlayerPoise = (float)Max;
	PlayerPoiseCd = TickNow() + 8;
	if (PS && PS->Stats) PS->Stats->StunUntil = FMath::Max(PS->Stats->StunUntil, (int64)TickNow() + 1);
	Say(TEXT("Your guard breaks! You are staggered."));
}

void UVenturesFeature::HookPlayerHurt(const UObject* WorldContext, int32 Amount)
{
	if (UVenturesFeature* V = Get(WorldContext)) { V->VocMarkFight(); V->IncomingGuard(Amount); }
}

void UVenturesFeature::TickGuard()
{
	const int32 Max = PlayerPoiseMax();
	if (PlayerPoise < 0.f) PlayerPoise = (float)Max;
	else if (PlayerPoise < Max && !(PlayerPoiseCd && TickNow() < PlayerPoiseCd)) PlayerPoise = FMath::Min((float)Max, PlayerPoise + 4.f);
}

void UVenturesFeature::TabGuard(FGPanelModel& M)
{
	M.Title = TEXT("Guardbreak");
	M.Lines.Add(TEXT("Armed foes have a small poise bar. Fill it with heavy hits, war fans, glaives, techniques or Tetherline to stagger them and open a riposte: tap Riposte, or land your next hit for an automatic crit."));
	M.Lines.Add(TEXT("Bosses keep Crestbreak as their big stagger. A Guardbreak on a boss feeds that bar and only stuns briefly, with a cooldown so they cannot be locked."));
	M.Lines.Add(FString::Printf(TEXT("Your poise: %d / %d (Defence and heavy armour). Breaks %d · ripostes %d."), FMath::RoundToInt(FMath::Max(0.f, PlayerPoise)), PlayerPoiseMax(), GuardBreaks, GuardRipostes));
	if (AAerondelMob* T = CurrentTarget())
	{
		if (GuardRiposteOpen(T)) VAct8(M, TEXT("Riposte"), [this, T] { GuardRiposte(T); }, true, true);
		else if (GuardArmed(T)) M.Lines.Add(FString::Printf(TEXT("Target poise: %d / %d."), GuardPoiseOf(T), GuardPoiseMax(T)));
	}
}
