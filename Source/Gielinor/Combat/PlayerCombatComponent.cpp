#include "Combat/PlayerCombatComponent.h"
#include "Combat/GielinorMob.h"
#include "Combat/CombatMath.h"
#include "Core/GielinorPlayerState.h"
#include "Data/GameDatabase.h"
#include "Data/SkillDef.h"
#include "Skills/SkillsComponent.h"
#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "GameFramework/Pawn.h"

UPlayerCombatComponent::UPlayerCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UPlayerCombatComponent::SetTarget(AGielinorMob* Mob) { Target = Mob; }
void UPlayerCombatComponent::ClearTarget() { Target = nullptr; }

bool UPlayerCombatComponent::ConsumeSwing()
{
	if (Cooldown > 0.f) return false;
	Cooldown = AttackSeconds * 0.5f; // VR swings may be a little faster than desktop auto-attack.
	return true;
}

void UPlayerCombatComponent::AwardXp(AGielinorPlayerState* PS, int32 Damage) const
{
	const UGameDatabase* Db = UGameDatabase::Get();
	if (!PS || !Db || Damage <= 0) return;
	const float Xp = UCombatMath::XpPerDamage * Damage;
	switch (Style)
	{
	case ECombatStyle::Accurate: PS->Skills->AddXp(Db->FindSkill(TEXT("attack")), Xp); break;
	case ECombatStyle::Aggressive: PS->Skills->AddXp(Db->FindSkill(TEXT("strength")), Xp); break;
	case ECombatStyle::Defensive: PS->Skills->AddXp(Db->FindSkill(TEXT("defence")), Xp); break;
	case ECombatStyle::Controlled:
		for (const TCHAR* S : { TEXT("attack"), TEXT("strength"), TEXT("defence") }) PS->Skills->AddXp(Db->FindSkill(S), Xp / 3.f);
		break;
	}
	PS->Skills->AddXp(Db->FindSkill(TEXT("hitpoints")), UCombatMath::HpXpPerDamage * Damage);
}

void UPlayerCombatComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* Fn)
{
	Super::TickComponent(Dt, TickType, Fn);
	Cooldown -= Dt;
	AGielinorMob* Mob = Target.Get();
	if (!Mob) return;
	if (Mob->GetState() == EMobState::Dead) { Target = nullptr; return; }
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;
	const float Dist = FVector::Dist2D(Pawn->GetActorLocation(), Mob->GetActorLocation());
	if (Dist > MeleeRange)
	{
		if (bAutoApproach && Pawn->GetController()) UAIBlueprintHelperLibrary::SimpleMoveToActor(Pawn->GetController(), Mob);
		return;
	}
	if (bAutoApproach && Cooldown <= 0.f)
	{
		FVector To = Mob->GetActorLocation() - Pawn->GetActorLocation(); To.Z = 0;
		if (!To.IsNearlyZero()) Pawn->SetActorRotation(To.Rotation());
		Mob->ReceivePlayerHit(Pawn, 1.f);
		Cooldown = AttackSeconds;
	}
}
