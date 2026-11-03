#include "Combat/PlayerCombatComponent.h"
#include "Player/AerondelCharacter.h"
#include "Combat/AerondelMob.h"
#include "Combat/HealthComponent.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Core/AerondelEvents.h"
#include "Data/GameDatabase.h"
#include "Data/ContentDefs.h"
#include "Data/MobDef.h"
#include "Data/SkillDef.h"
#include "Data/ItemDef.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Features/TrueAimFeature.h"
#include "Engine/GameInstance.h"

#define LOCTEXT_NAMESPACE "AerondelCombat"

UPlayerCombatComponent::UPlayerCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

AAerondelPlayerState* UPlayerCombatComponent::PS() const { return AAerondelPlayerState::FromActor(GetOwner()); }
void UPlayerCombatComponent::Msg(const FText& T) const { if (UAerondelEvents* Ev = UAerondelEvents::Get(this)) Ev->Message(T); }

void UPlayerCombatComponent::GiveXp(const TMap<FName, float>& Xp) const
{
	AAerondelPlayerState* S = PS();
	const UGameDatabase* Db = UGameDatabase::Get();
	if (!S || !Db) return;
	for (const auto& X : Xp) if (X.Value > 0.f) S->Skills->AddXp(Db->FindSkill(X.Key), X.Value * UAerondelRules::XpMultiplier(S, X.Key));
}

void UPlayerCombatComponent::SetTarget(AAerondelMob* Mob) { Target = Mob; }
void UPlayerCombatComponent::ClearTarget() { Target = nullptr; }

float UPlayerCombatComponent::AttackRangeCm() const
{
	AAerondelPlayerState* S = PS();
	const bool bMagic = S && S->Stats && !S->Stats->AutocastSpell.IsNone();
	const float Tiles = bMagic ? 10.f : UAerondelRules::AttackRangeTiles(S);
	return Tiles * UAerondelRules::TileCm + ReachSlackCm;
}

bool UPlayerCombatComponent::ConsumeSwing()
{
	if (Cooldown > 0.f) return false;
	AAerondelPlayerState* S = PS();
	if (S && S->Stats && S->Stats->IsStunned()) return false;
	// VR swings may be a little faster than desktop auto-attack.
	Cooldown = UAerondelRules::AttackSpeedTicks(S) * UAerondelRules::TickSeconds * 0.5f;
	return true;
}

FText UPlayerCombatComponent::UseAbility(FName AbilityId)
{
	AAerondelPlayerState* S = PS();
	if (!S || !S->Stats) return FText::GetEmpty();
	AAerondelMob* Mob = Target.Get();
	// True Aim: attack abilities fire as a shape where you face instead of riding on the next swing.
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UTrueAimFeature* Aim = GI ? GI->GetSubsystem<UTrueAimFeature>() : nullptr;
	const UGameDatabase* Db = UGameDatabase::Get();
	const UAbilityDef* Def = Db ? Db->FindAbility(AbilityId) : nullptr;
	if (Aim && Aim->IsOn() && Def && Def->Style != TEXT("magic"))
	{
		const FText AimErr = S->Stats->UseAbility(AbilityId, true);
		if (!AimErr.IsEmpty()) { Msg(AimErr); return AimErr; }
		if (S->Stats->QueuedAbility == AbilityId) { S->Stats->ConsumeQueuedAbility(); Aim->Strike(AbilityId); }
		return FText::GetEmpty();
	}
	const FText Err = S->Stats->UseAbility(AbilityId, Mob && !Mob->IsDead());
	if (!Err.IsEmpty()) Msg(Err);
	else if (Mob && S->Stats->QueuedAbility == AbilityId && Cooldown > UAerondelRules::TickSeconds) Cooldown = UAerondelRules::TickSeconds;
	return Err;
}

void UPlayerCombatComponent::OnAttackedBy(AAerondelMob* Mob)
{
	if (AAerondelCharacter* Ch = Cast<AAerondelCharacter>(GetOwner())) Ch->PlayBodyAnim(TEXT("hit"));
	AAerondelPlayerState* S = PS();
	if (!Target.IsValid() && (!S || !S->Stats || S->Stats->bAutoRetaliate)) SetTarget(Mob);
}

bool UPlayerCombatComponent::CastAt(AAerondelMob* Mob, USpellDef* Spell)
{
	AAerondelPlayerState* S = PS();
	FText Why;
	if (!UAerondelRules::CanCast(S, Spell, Why)) { Msg(Why); return false; }
	const bool bSaved = FMath::FRand() < UAerondelRules::Perk(S, TEXT("magic"), TEXT("runeSave"));
	if (!bSaved) for (const TPair<UItemDef*, int32>& R : UAerondelRules::RuneCost(S, Spell)) S->Inventory->Remove(R.Key, R.Value);
	else Msg(LOCTEXT("RuneSave", "Your magic saves your runes."));
	const UMobDef* Def = Mob->Definition;
	int32 Dmg = 0;
	if (FMath::FRand() < UAerondelRules::MagicAccuracy(S, Def))
	{
		Dmg = 1 + FMath::FloorToInt(FMath::FRand() * UAerondelRules::MagicMaxHit(S, Spell));
		const FName Vs = Def ? FName(*(TEXT("dmgVs.") + Def->Id.ToString())) : NAME_None;
		Dmg = FMath::FloorToInt(Dmg * (1.f + UAerondelRules::QuestFx(S, Vs) + UAerondelRules::QuestFx(S, TEXT("dmgVs.*")) + UAerondelRules::GearFx(S, TEXT("dmgPct"))));
		if (UAerondelRules::WeakTo(Def, UAerondelRules::SpellElement(Spell))) Dmg = FMath::FloorToInt(Dmg * UAerondelRules::WeakMult);
	}
	// Magic kit: Surge, then any queued magic ability riding on this cast.
	UPlayerStatsComponent* St = S->Stats;
	if (St->SurgeLeft > 0 && Dmg > 0) { Dmg = FMath::FloorToInt(Dmg * 1.5f); --St->SurgeLeft; }
	Dmg += MagicKit(Mob, Spell);
	ApplyHex(Mob, Spell, Dmg);
	Dmg = FMath::Min(Dmg, Mob->GetHp());
	TMap<FName, float> Xp;
	Xp.Add(TEXT("magic"), Spell->Xp * 2.f + Dmg * 0.6f);
	Xp.Add(TEXT("hitpoints"), Dmg * 0.4f);
	GiveXp(Xp);
	const bool bAlive = Mob->GetHp() - Dmg > 0;
	Mob->TakePlayerDamage(Dmg, GetOwner());
	// Relicry/Delving gear: frost freezes for 3 s (5 ticks); chain arcs to another foe within 6 m for half damage.
	if (Dmg > 0 && bAlive && FMath::FRand() < UAerondelRules::GearFx(S, TEXT("frost"))) { Mob->Stun(5); Msg(FText::Format(LOCTEXT("Frost", "The {0} is frozen in place!"), Mob->GetDisplayName())); }
	if (Dmg > 1 && FMath::FRand() < UAerondelRules::GearFx(S, TEXT("chain")))
	{
		const FVector C = Mob->GetActorLocation();
		for (TActorIterator<AAerondelMob> It(GetWorld()); It; ++It)
		{
			if (*It == Mob || It->IsDead() || It->GetHp() <= 0 || FVector::Dist2D(It->GetActorLocation(), C) > 600.f) continue;
			const int32 D2 = FMath::Min(It->GetHp(), Dmg / 2);
			It->TakePlayerDamage(D2, GetOwner()); Msg(LOCTEXT("Chain", "Your spell arcs to another foe!"));
			break;
		}
	}
	return true;
}

int32 UPlayerCombatComponent::MagicKit(AAerondelMob* Mob, USpellDef* Spell)
{
	AAerondelPlayerState* S = PS();
	UPlayerStatsComponent* St = S ? S->Stats : nullptr;
	const UGameDatabase* Db = UGameDatabase::Get();
	if (!St || !Db || St->QueuedAbility.IsNone()) return 0;
	const UAbilityDef* A = Db->FindAbility(St->QueuedAbility);
	if (!A || A->Style != TEXT("magic")) return 0;
	const FName Special = St->ConsumeQueuedAbility(); // starts the cooldown
	for (const FRuneCost& R : A->Runes)
		if (S->Inventory->CountOf(R.Rune) < R.Amount) { Msg(FText::Format(LOCTEXT("KitRunes", "Not enough runes for {0}."), A->DisplayName)); return 0; }
	for (const FRuneCost& R : A->Runes) S->Inventory->Remove(R.Rune, R.Amount);
	Msg(FText::Format(LOCTEXT("Kit", "{0}!"), A->DisplayName));
	const int32 Mh = UAerondelRules::MagicMaxHit(S, Spell);
	int32 Extra = 0;
	if (!A->Element.IsNone())
	{
		const float K = A->Element == TEXT("fire") ? 1.1f : A->Element == TEXT("earth") ? 1.f : 0.9f;
		Extra = FMath::FloorToInt(Mh * (0.4f + FMath::FRand() * 0.5f) * K);
		if (UAerondelRules::WeakTo(Mob->Definition, A->Element))
		{
			Extra = FMath::FloorToInt(Extra * UAerondelRules::WeakMult);
			Msg(FText::Format(LOCTEXT("Weak", "The {0} is weak to {1}!"), Mob->GetDisplayName(), FText::FromName(A->Element)));
		}
		if (A->Element == TEXT("earth")) Mob->Wither(8);
		if (A->Element == TEXT("fire")) Mob->ApplyVenom(FMath::Max(4, FMath::RoundToInt(Mh * 0.08f)));
	}
	else if (A->bScale) // v22 high-level Magic techniques: grow with Magic level, x(1 + Magic/500)
	{
		const float Kx = 1.f + (S->Skills ? S->Skills->GetLevelById(TEXT("magic")) : 1) / 500.f;
		if (Special == TEXT("frostspire")) { Extra = FMath::FloorToInt(Mh * 1.2f * Kx); Mob->Stun(4); Msg(FText::Format(LOCTEXT("Frozen", "The {0} is frozen solid!"), Mob->GetDisplayName())); }
		else if (Special == TEXT("leyrend")) { Extra = FMath::FloorToInt(Mh * (1.5f + FMath::FRand()) * Kx); Mob->Wither(10); }
		else if (Special == TEXT("astralnova"))
		{
			Extra = FMath::FloorToInt(Mh * 2.f * Kx);
			const FVector C = Mob->GetActorLocation();
			TArray<AAerondelMob*> Near;
			for (TActorIterator<AAerondelMob> It(GetWorld()); It; ++It)
				if (*It != Mob && !It->IsDead() && It->GetHp() > 0 && FVector::Dist2D(It->GetActorLocation(), C) < 600.f) Near.Add(*It);
			for (AAerondelMob* M : Near)
			{
				const int32 D = FMath::Min(M->GetHp(), FMath::FloorToInt(Mh * 1.2f * Kx * (0.6f + FMath::FRand() * 0.4f)));
				TMap<FName, float> Xp; Xp.Add(TEXT("magic"), D * 0.6f); GiveXp(Xp);
				M->TakePlayerDamage(D, GetOwner());
			}
		}
		else if (Special == TEXT("voidcollapse")) Extra = FMath::FloorToInt(Mh * 5.f * Kx * (Mob->GetHp() < Mob->GetMaxHp() * 0.25f ? 2.f : 1.f));
	}
	else if (Special == TEXT("surge")) { St->SurgeLeft = 3; Msg(LOCTEXT("Surge", "Power surges through your spells (+50% for 3 casts).")); }
	else if (Special == TEXT("storm"))
	{
		Extra = FMath::FloorToInt(Mh * (0.8f + FMath::FRand() * 0.7f));
		const FVector C = Mob->GetActorLocation();
		TArray<AAerondelMob*> Near;
		for (TActorIterator<AAerondelMob> It(GetWorld()); It; ++It)
			if (*It != Mob && !It->IsDead() && It->GetHp() > 0 && FVector::Dist2D(It->GetActorLocation(), C) < 500.f) Near.Add(*It);
		for (AAerondelMob* M : Near)
		{
			const int32 D = FMath::Min(M->GetHp(), FMath::FloorToInt(Mh * (0.5f + FMath::FRand() * 0.5f)));
			TMap<FName, float> Xp; Xp.Add(TEXT("magic"), D * 0.6f); GiveXp(Xp);
			M->TakePlayerDamage(D, GetOwner());
		}
	}
	return Extra;
}

void UPlayerCombatComponent::ApplyHex(AAerondelMob* Mob, USpellDef* Spell, int32 Dmg)
{
	if (!Spell || Spell->Hex.IsNone() || !Mob || Mob->IsDead()) return;
	if (Spell->Hex == TEXT("rot")) { Mob->ApplyVenom(FMath::Max(4, FMath::RoundToInt(Spell->MaxHit * 0.2f))); Msg(FText::Format(LOCTEXT("Rot", "The {0} begins to rot."), Mob->GetDisplayName())); }
	else if (Spell->Hex == TEXT("bind")) { Mob->Stun(4); Msg(FText::Format(LOCTEXT("Bind", "Roots of bog-rope bind the {0}!"), Mob->GetDisplayName())); }
	else if (Spell->Hex == TEXT("wither")) { Mob->Wither(33); Msg(FText::Format(LOCTEXT("Wither", "The {0} withers. It hits softer."), Mob->GetDisplayName())); }
	else if (Spell->Hex == TEXT("leech") && Dmg > 0) if (AAerondelPlayerState* S = PS()) S->Health->Heal(Dmg / 2);
}

void UPlayerCombatComponent::PlayerAttack(AAerondelMob* Mob, float Power)
{
	AAerondelPlayerState* S = PS();
	if (!S || !S->Stats || !Mob || Mob->IsDead()) return;
	UPlayerStatsComponent* St = S->Stats;
	const UGameDatabase* Db = UGameDatabase::Get();
	if (AAerondelCharacter* Ch = Cast<AAerondelCharacter>(GetOwner())) Ch->PlayBodyAnim(TEXT("attack"));

	// Magic autocast.
	if (!St->AutocastSpell.IsNone() && Db)
	{
		if (USpellDef* Spell = Db->FindSpell(St->AutocastSpell))
		{
			if (!CastAt(Mob, Spell)) ClearTarget();
			return;
		}
	}

	const bool bRanged = UAerondelRules::IsRanged(S);
	const FName Special = St->ConsumeQueuedAbility();
	FText AmmoErr;
	if (bRanged && !St->UseAmmo(AmmoErr)) { Msg(AmmoErr); ClearTarget(); return; }

	FSwingMods Base;
	if (St->IsRaging()) { Base.Accuracy = 0.2f; Base.Damage = 0.2f; }
	Base.Damage += Power - 1.f;
	TArray<FSwingMods> Swings;
	auto With = [&Base](float A, float D) { FSwingMods M = Base; M.Accuracy += A; M.Damage += D; return M; };
	if (Special == TEXT("power")) Swings.Add(With(0.25f, 0.6f));
	else if (Special == TEXT("puncture")) { Swings.Add(With(0.1f, 0.f)); Swings.Add(With(0.1f, 0.f)); }
	else if (Special == TEXT("bash")) Swings.Add(With(0.3f, -0.5f));
	else if (Special == TEXT("snipe")) Swings.Add(With(0.3f, 1.f));
	else if (Special == TEXT("rend")) Swings.Add(With(0.f, 0.9f));
	else if (Special == TEXT("flurry")) for (int32 i = 0; i < 3; ++i) Swings.Add(With(0.f, -0.15f));
	else if (Special == TEXT("challenge")) Swings.Add(Base);
	else if (Special == TEXT("volley"))
	{
		Swings.Add(With(-0.15f, 0.f));
		for (int32 i = 0; i < 2; ++i) { FText E; if (St->UseAmmo(E)) Swings.Add(With(-0.15f, 0.f)); }
	}
	else Swings.Add(Base);
	if (bRanged)
	{
		const int32 Multi = FMath::RoundToInt(UAerondelRules::GearFx(S, TEXT("multiArrow")));
		for (int32 i = 1; i < Multi; ++i) { FText E; if (St->UseAmmo(E)) Swings.Add(Base); }
		if (Special.IsNone() && FMath::FRand() < UAerondelRules::Perk(S, TEXT("ranged"), TEXT("doubleShot"))) { Swings.Add(Base); Msg(LOCTEXT("Double", "Double Shot!")); }
		if (FMath::FRand() < UAerondelRules::GearFx(S, TEXT("echo"))) { Swings.Add(With(0.f, -0.4f)); Msg(LOCTEXT("Echo", "A free echo arrow follows!")); } // gear: echo at 60% power
	}
	ArsenalSwing(Mob, Swings, bRanged, Special);
	float Next = UAerondelRules::AttackSpeedTicks(S) * UAerondelRules::TickSeconds;
	if (!bRanged && FMath::FRand() < UAerondelRules::Perk(S, TEXT("attack"), TEXT("quick"))) Next = FMath::Max(UAerondelRules::TickSeconds, Next - UAerondelRules::TickSeconds);
	Cooldown = Next;
	if (!Special.IsNone() && Db) if (const UAbilityDef* A = Db->FindAbility(Special)) Msg(FText::Format(LOCTEXT("Ab", "{0}!"), A->DisplayName));

	int32 Total = 0;
	bool bExtraUsed = false;
	for (int32 i = 0; i < Swings.Num() && i < 9; ++i) // 9: Thousand Cuts (6) plus class extras
	{
		if (Mob->IsDead() || Mob->GetHp() <= 0) break;
		FSwingMods M = Swings[i];
		const int32 D = UAerondelRules::RollPlayerHit(S, Mob->Definition, Mob->GetHp(), Mob->GetMaxHp(), M);
		Total += D;
		if (D > 0) St->GainAdrenaline(Special.IsNone() ? 4.f : 0.f);
		if (M.bBigHit) Msg(LOCTEXT("Big", "Your weapon unleashes its special!"));
		const FWeaponProcs P = UAerondelRules::RollWeaponProcs(S, D);
		if (P.bAgain && !bExtraUsed) { bExtraUsed = true; Swings.Add(Base); Msg(LOCTEXT("Twice", "Your weapon strikes twice!")); }
		if (P.Heal > 0) S->Health->Heal(P.Heal);
		const bool bAlive = Mob->GetHp() - D > 0;
		Mob->TakePlayerDamage(D, GetOwner());
		if (bAlive && P.Poison > 0 && Mob->ApplyVenom(P.Poison)) Msg(FText::Format(LOCTEXT("Venom", "You poison the {0}!"), Mob->GetDisplayName()));
		if (bAlive && !bRanged && FMath::FRand() < UAerondelRules::GearFx(S, TEXT("burn")) && Mob->ApplyVenom(FMath::Max(1, FMath::RoundToInt(UAerondelRules::GearFx(S, TEXT("burnDmg")))))) Msg(FText::Format(LOCTEXT("Burn", "You set the {0} ablaze!"), Mob->GetDisplayName()));
		if (bAlive && P.bStun) { Mob->Stun(5); Msg(FText::Format(LOCTEXT("Stun", "The {0} is stunned!"), Mob->GetDisplayName())); }
	}
	if (Special == TEXT("challenge") && !Mob->IsDead()) { Mob->Challenge(4, 10); Msg(FText::Format(LOCTEXT("Challenge", "You shove the {0} back on its heels; it only has eyes for you."), Mob->GetDisplayName())); }
	if (Special == TEXT("bash") && !Mob->IsDead()) { Mob->Stun(5); Msg(FText::Format(LOCTEXT("Stun", "The {0} is stunned!"), Mob->GetDisplayName())); }
	const UItemDef* Wpn = S->Equipment ? S->Equipment->GetEquipped(EEquipSlot::Weapon) : nullptr;
	if (Special == TEXT("whirl") && !bRanged) // Reaping Whirl: every other foe within 4 m takes a cut too (5 m with a glaive)
	{
		const float R = Wpn && Wpn->WeaponClass == TEXT("glaive") ? 500.f : 400.f;
		const FVector P0 = GetOwner()->GetActorLocation();
		TArray<AAerondelMob*> Near;
		for (TActorIterator<AAerondelMob> It(GetWorld()); It; ++It)
			if (*It != Mob && !It->IsDead() && It->GetHp() > 0 && FVector::Dist2D(It->GetActorLocation(), P0) < R) Near.Add(*It);
		FSwingMods M = Base; M.Damage -= 0.1f;
		for (AAerondelMob* O : Near)
		{
			const int32 D = UAerondelRules::RollPlayerHit(S, O->Definition, O->GetHp(), O->GetMaxHp(), M);
			Total += D;
			O->TakePlayerDamage(D, GetOwner());
		}
		if (Near.Num()) Msg(FText::Format(LOCTEXT("WhirlN", "Reaping Whirl catches {0} more {0}|plural(one=foe,other=foes)!"), Near.Num()));
	}
	TMap<FName, float> Xp = UAerondelRules::CombatXp(S, Total);
	if (!bRanged && Total > 0 && Wpn && Wpn->IsBlade()) Xp.FindOrAdd(TEXT("swordsmanship")) += Total * 0.6f; // arsenalXp
	GiveXp(Xp);
}

void UPlayerCombatComponent::ArsenalSwing(AAerondelMob* Mob, TArray<FSwingMods>& Swings, bool bRanged, FName Special)
{
	// Port of Gielinor-Lite js/arsenal.js arsenalSwing (v22 techniques, weapon-class passives, Swordsmanship blade perks).
	AAerondelPlayerState* S = PS();
	if (!S || !Mob || Swings.Num() == 0) return;
	UPlayerStatsComponent* St = S->Stats;
	const UItemDef* W = S->Equipment ? S->Equipment->GetEquipped(EEquipSlot::Weapon) : nullptr;
	const FName Cls = W ? W->WeaponClass : NAME_None;
	auto Extra = [&Swings](float Dmg) { FSwingMods M = Swings[0]; M.Damage += Dmg; Swings.Add(M); };
	if (Special == TEXT("iaido")) { Swings[0].Damage += Cls == TEXT("katana") ? 2.5f : 1.5f; Swings[0].Accuracy += 0.4f; }
	else if (Special == TEXT("crescent")) { Swings[0].Damage += 0.2f; const int32 N = Cls == TEXT("scimitar") ? 2 : 1; for (int32 i = 0; i < N; ++i) Extra(0.f); }
	else if (Special == TEXT("whirl")) { Swings[0].Damage -= 0.1f; const int32 N = Cls == TEXT("glaive") ? 4 : 3; for (int32 i = 0; i < N; ++i) Extra(0.f); }
	else if (Special == TEXT("gale")) { Swings[0].Accuracy += 0.3f; Mob->Stun(Cls == TEXT("warfan") ? 8 : 5); Msg(FText::Format(LOCTEXT("Gale", "A gale pins the {0} in place!"), Mob->GetDisplayName())); }
	else if (Special == TEXT("ricochet")) { const float B = Cls == TEXT("chakram") ? 0.4f : 0.25f; Extra(B); Extra(B * 2.f); }
	else if (Special == TEXT("starfall")) { Swings[0].Damage += 0.3f; for (int32 i = 0; i < 3; ++i) { FText E; if (St && St->UseAmmo(E)) Extra(0.f); } } // each extra shot uses ammo
	else if (Special == TEXT("eclipse")) { Swings[0].Damage += Mob->GetHp() < Mob->GetMaxHp() * 0.15f ? 9.f : 4.f; Swings[0].Accuracy += 0.5f; }
	else if (Special == TEXT("thousand")) { Swings[0].Damage += 0.1f; Swings[0].Accuracy += 0.2f; for (int32 i = 0; i < 5; ++i) Extra(0.f); }
	if (bRanged || Cls.IsNone()) return;
	const bool bSameMob = ClassMob.Get() == Mob && ClassName == Cls;
	if (Cls == TEXT("katana")) { if (!bSameMob) { ClassHits = 0; Swings[0].Damage += 0.4f; Msg(LOCTEXT("IaidoOpen", "Iaido opening!")); } ++ClassHits; }
	else if (Cls == TEXT("scimitar")) { ClassHits = bSameMob ? ClassHits + 1 : 1; if (ClassHits % 4 == 0) { Extra(-0.2f); Msg(LOCTEXT("Flourish", "Scimitar flourish!")); } }
	else if (Cls == TEXT("glaive")) { if (FMath::FRand() < 0.2f) { Extra(-0.3f); Msg(LOCTEXT("Sweep", "Glaive sweep!")); } }
	else if (Cls == TEXT("warfan")) { if (St) St->GainAdrenaline(3.f); if (FMath::FRand() < 0.1f) { Mob->Stun(3); Msg(LOCTEXT("Gust", "A gust staggers the foe!")); } }
	else if (Cls == TEXT("chakram")) { if (FMath::FRand() < 0.25f) { Extra(-0.2f); Msg(LOCTEXT("Chakram", "The chakram returns for another cut!")); } }
	ClassMob = Mob; ClassName = Cls;
	if (W->IsBlade())
	{
		const float A = UAerondelRules::Perk(S, TEXT("swordsmanship"), TEXT("bladeAcc"));
		const float D = UAerondelRules::Perk(S, TEXT("swordsmanship"), TEXT("bladeDmg"));
		if (A != 0.f || D != 0.f) for (FSwingMods& M : Swings) { M.Accuracy += A; M.Damage += D; }
	}
}

void UPlayerCombatComponent::BonusHit(AAerondelMob* Mob, const FText& Label)
{
	AAerondelPlayerState* S = PS();
	if (!S || !Mob || Mob->IsDead()) return;
	FSwingMods M;
	const int32 D = UAerondelRules::RollPlayerHit(S, Mob->Definition, Mob->GetHp(), Mob->GetMaxHp(), M);
	GiveXp(UAerondelRules::CombatXp(S, D));
	if (D > 0) Msg(FText::Format(LOCTEXT("Bonus", "{0}!"), Label));
	Mob->TakePlayerDamage(D, GetOwner());
}

void UPlayerCombatComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* Fn)
{
	Super::TickComponent(Dt, TickType, Fn);
	Cooldown -= Dt;
	AAerondelMob* Mob = Target.Get();
	if (!Mob) return;
	if (Mob->IsDead()) { Target = nullptr; return; }
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;
	const float Dist = FVector::Dist2D(Pawn->GetActorLocation(), Mob->GetActorLocation());
	if (Dist > AttackRangeCm())
	{
		if (bAutoApproach && Pawn->GetController()) UAIBlueprintHelperLibrary::SimpleMoveToActor(Pawn->GetController(), Mob);
		return;
	}
	AAerondelPlayerState* S = PS();
	if (bAutoApproach && Cooldown <= 0.f && !(S && S->Stats && S->Stats->IsStunned()))
	{
		if (Pawn->GetController()) Pawn->GetController()->StopMovement();
		FVector To = Mob->GetActorLocation() - Pawn->GetActorLocation(); To.Z = 0;
		if (!To.IsNearlyZero()) Pawn->SetActorRotation(To.Rotation());
		PlayerAttack(Mob, 1.f);
		if (Cooldown <= 0.f) Cooldown = UAerondelRules::AttackSpeedTicks(S) * UAerondelRules::TickSeconds;
	}
}

#undef LOCTEXT_NAMESPACE
