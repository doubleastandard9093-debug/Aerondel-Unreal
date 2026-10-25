#include "Combat/PlayerCombatComponent.h"
#include "Combat/GielinorMob.h"
#include "Combat/HealthComponent.h"
#include "Core/GielinorPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Core/GielinorEvents.h"
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

#define LOCTEXT_NAMESPACE "GielinorCombat"

UPlayerCombatComponent::UPlayerCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

AGielinorPlayerState* UPlayerCombatComponent::PS() const { return AGielinorPlayerState::FromActor(GetOwner()); }
void UPlayerCombatComponent::Msg(const FText& T) const { if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->Message(T); }

void UPlayerCombatComponent::GiveXp(const TMap<FName, float>& Xp) const
{
	AGielinorPlayerState* S = PS();
	const UGameDatabase* Db = UGameDatabase::Get();
	if (!S || !Db) return;
	for (const auto& X : Xp) if (X.Value > 0.f) S->Skills->AddXp(Db->FindSkill(X.Key), X.Value * UGielinorRules::XpMultiplier(S, X.Key));
}

void UPlayerCombatComponent::SetTarget(AGielinorMob* Mob) { Target = Mob; }
void UPlayerCombatComponent::ClearTarget() { Target = nullptr; }

float UPlayerCombatComponent::AttackRangeCm() const
{
	AGielinorPlayerState* S = PS();
	const bool bMagic = S && S->Stats && !S->Stats->AutocastSpell.IsNone();
	const float Tiles = bMagic ? 10.f : UGielinorRules::AttackRangeTiles(S);
	return Tiles * UGielinorRules::TileCm + ReachSlackCm;
}

bool UPlayerCombatComponent::ConsumeSwing()
{
	if (Cooldown > 0.f) return false;
	AGielinorPlayerState* S = PS();
	if (S && S->Stats && S->Stats->IsStunned()) return false;
	// VR swings may be a little faster than desktop auto-attack.
	Cooldown = UGielinorRules::AttackSpeedTicks(S) * UGielinorRules::TickSeconds * 0.5f;
	return true;
}

FText UPlayerCombatComponent::UseAbility(FName AbilityId)
{
	AGielinorPlayerState* S = PS();
	if (!S || !S->Stats) return FText::GetEmpty();
	AGielinorMob* Mob = Target.Get();
	const FText Err = S->Stats->UseAbility(AbilityId, Mob && !Mob->IsDead());
	if (!Err.IsEmpty()) Msg(Err);
	else if (Mob && S->Stats->QueuedAbility == AbilityId && Cooldown > UGielinorRules::TickSeconds) Cooldown = UGielinorRules::TickSeconds;
	return Err;
}

void UPlayerCombatComponent::OnAttackedBy(AGielinorMob* Mob)
{
	AGielinorPlayerState* S = PS();
	if (!Target.IsValid() && (!S || !S->Stats || S->Stats->bAutoRetaliate)) SetTarget(Mob);
}

bool UPlayerCombatComponent::CastAt(AGielinorMob* Mob, USpellDef* Spell)
{
	AGielinorPlayerState* S = PS();
	FText Why;
	if (!UGielinorRules::CanCast(S, Spell, Why)) { Msg(Why); return false; }
	const bool bSaved = FMath::FRand() < UGielinorRules::Perk(S, TEXT("magic"), TEXT("runeSave"));
	if (!bSaved) for (const TPair<UItemDef*, int32>& R : UGielinorRules::RuneCost(S, Spell)) S->Inventory->Remove(R.Key, R.Value);
	else Msg(LOCTEXT("RuneSave", "Your magic saves your runes."));
	const UMobDef* Def = Mob->Definition;
	int32 Dmg = 0;
	if (FMath::FRand() < UGielinorRules::MagicAccuracy(S, Def))
	{
		Dmg = 1 + FMath::FloorToInt(FMath::FRand() * UGielinorRules::MagicMaxHit(S, Spell));
		const FName Vs = Def ? FName(*(TEXT("dmgVs.") + Def->Id.ToString())) : NAME_None;
		Dmg = FMath::FloorToInt(Dmg * (1.f + UGielinorRules::QuestFx(S, Vs) + UGielinorRules::QuestFx(S, TEXT("dmgVs.*")) + UGielinorRules::GearFx(S, TEXT("dmgPct"))));
	}
	Dmg = FMath::Min(Dmg, Mob->GetHp());
	TMap<FName, float> Xp;
	Xp.Add(TEXT("magic"), Spell->Xp * 2.f + Dmg * 0.6f);
	Xp.Add(TEXT("hitpoints"), Dmg * 0.4f);
	GiveXp(Xp);
	Mob->TakePlayerDamage(Dmg, GetOwner());
	return true;
}

void UPlayerCombatComponent::PlayerAttack(AGielinorMob* Mob, float Power)
{
	AGielinorPlayerState* S = PS();
	if (!S || !S->Stats || !Mob || Mob->IsDead()) return;
	UPlayerStatsComponent* St = S->Stats;
	const UGameDatabase* Db = UGameDatabase::Get();

	// Magic autocast.
	if (!St->AutocastSpell.IsNone() && Db)
	{
		if (USpellDef* Spell = Db->FindSpell(St->AutocastSpell))
		{
			if (!CastAt(Mob, Spell)) ClearTarget();
			return;
		}
	}

	const bool bRanged = UGielinorRules::IsRanged(S);
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
	else if (Special == TEXT("volley"))
	{
		Swings.Add(With(-0.15f, 0.f));
		for (int32 i = 0; i < 2; ++i) { FText E; if (St->UseAmmo(E)) Swings.Add(With(-0.15f, 0.f)); }
	}
	else Swings.Add(Base);
	if (bRanged)
	{
		const int32 Multi = FMath::RoundToInt(UGielinorRules::GearFx(S, TEXT("multiArrow")));
		for (int32 i = 1; i < Multi; ++i) { FText E; if (St->UseAmmo(E)) Swings.Add(Base); }
		if (Special.IsNone() && FMath::FRand() < UGielinorRules::Perk(S, TEXT("ranged"), TEXT("doubleShot"))) { Swings.Add(Base); Msg(LOCTEXT("Double", "Double Shot!")); }
	}
	float Next = UGielinorRules::AttackSpeedTicks(S) * UGielinorRules::TickSeconds;
	if (!bRanged && FMath::FRand() < UGielinorRules::Perk(S, TEXT("attack"), TEXT("quick"))) Next = FMath::Max(UGielinorRules::TickSeconds, Next - UGielinorRules::TickSeconds);
	Cooldown = Next;
	if (!Special.IsNone() && Db) if (const UAbilityDef* A = Db->FindAbility(Special)) Msg(FText::Format(LOCTEXT("Ab", "{0}!"), A->DisplayName));

	int32 Total = 0;
	bool bExtraUsed = false;
	for (int32 i = 0; i < Swings.Num() && i < 6; ++i)
	{
		if (Mob->IsDead() || Mob->GetHp() <= 0) break;
		FSwingMods M = Swings[i];
		const int32 D = UGielinorRules::RollPlayerHit(S, Mob->Definition, Mob->GetHp(), Mob->GetMaxHp(), M);
		Total += D;
		if (D > 0) St->GainAdrenaline(Special.IsNone() ? 4.f : 0.f);
		if (M.bBigHit) Msg(LOCTEXT("Big", "Your weapon unleashes its special!"));
		const FWeaponProcs P = UGielinorRules::RollWeaponProcs(S, D);
		if (P.bAgain && !bExtraUsed) { bExtraUsed = true; Swings.Add(Base); Msg(LOCTEXT("Twice", "Your weapon strikes twice!")); }
		if (P.Heal > 0) S->Health->Heal(P.Heal);
		const bool bAlive = Mob->GetHp() - D > 0;
		Mob->TakePlayerDamage(D, GetOwner());
		if (bAlive && P.Poison > 0 && Mob->ApplyVenom(P.Poison)) Msg(FText::Format(LOCTEXT("Venom", "You poison the {0}!"), Mob->GetDisplayName()));
		if (bAlive && P.bStun) { Mob->Stun(5); Msg(FText::Format(LOCTEXT("Stun", "The {0} is stunned!"), Mob->GetDisplayName())); }
	}
	if (Special == TEXT("bash") && !Mob->IsDead()) { Mob->Stun(5); Msg(FText::Format(LOCTEXT("Stun", "The {0} is stunned!"), Mob->GetDisplayName())); }
	GiveXp(UGielinorRules::CombatXp(S, Total));
}

void UPlayerCombatComponent::BonusHit(AGielinorMob* Mob, const FText& Label)
{
	AGielinorPlayerState* S = PS();
	if (!S || !Mob || Mob->IsDead()) return;
	FSwingMods M;
	const int32 D = UGielinorRules::RollPlayerHit(S, Mob->Definition, Mob->GetHp(), Mob->GetMaxHp(), M);
	GiveXp(UGielinorRules::CombatXp(S, D));
	if (D > 0) Msg(FText::Format(LOCTEXT("Bonus", "{0}!"), Label));
	Mob->TakePlayerDamage(D, GetOwner());
}

void UPlayerCombatComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* Fn)
{
	Super::TickComponent(Dt, TickType, Fn);
	Cooldown -= Dt;
	AGielinorMob* Mob = Target.Get();
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
	AGielinorPlayerState* S = PS();
	if (bAutoApproach && Cooldown <= 0.f && !(S && S->Stats && S->Stats->IsStunned()))
	{
		if (Pawn->GetController()) Pawn->GetController()->StopMovement();
		FVector To = Mob->GetActorLocation() - Pawn->GetActorLocation(); To.Z = 0;
		if (!To.IsNearlyZero()) Pawn->SetActorRotation(To.Rotation());
		PlayerAttack(Mob, 1.f);
		if (Cooldown <= 0.f) Cooldown = UGielinorRules::AttackSpeedTicks(S) * UGielinorRules::TickSeconds;
	}
}

#undef LOCTEXT_NAMESPACE
