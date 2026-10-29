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
#include "EngineUtils.h"
#include "Features/TrueAimFeature.h"
#include "Engine/GameInstance.h"

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
		if (UGielinorRules::WeakTo(Def, UGielinorRules::SpellElement(Spell))) Dmg = FMath::FloorToInt(Dmg * UGielinorRules::WeakMult);
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
	Mob->TakePlayerDamage(Dmg, GetOwner());
	return true;
}

int32 UPlayerCombatComponent::MagicKit(AGielinorMob* Mob, USpellDef* Spell)
{
	AGielinorPlayerState* S = PS();
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
	const int32 Mh = UGielinorRules::MagicMaxHit(S, Spell);
	int32 Extra = 0;
	if (!A->Element.IsNone())
	{
		const float K = A->Element == TEXT("fire") ? 1.1f : A->Element == TEXT("earth") ? 1.f : 0.9f;
		Extra = FMath::FloorToInt(Mh * (0.4f + FMath::FRand() * 0.5f) * K);
		if (UGielinorRules::WeakTo(Mob->Definition, A->Element))
		{
			Extra = FMath::FloorToInt(Extra * UGielinorRules::WeakMult);
			Msg(FText::Format(LOCTEXT("Weak", "The {0} is weak to {1}!"), Mob->GetDisplayName(), FText::FromName(A->Element)));
		}
		if (A->Element == TEXT("earth")) Mob->Wither(8);
		if (A->Element == TEXT("fire")) Mob->ApplyVenom(FMath::Max(4, FMath::RoundToInt(Mh * 0.08f)));
	}
	else if (Special == TEXT("surge")) { St->SurgeLeft = 3; Msg(LOCTEXT("Surge", "Power surges through your spells (+50% for 3 casts).")); }
	else if (Special == TEXT("storm"))
	{
		Extra = FMath::FloorToInt(Mh * (0.8f + FMath::FRand() * 0.7f));
		const FVector C = Mob->GetActorLocation();
		TArray<AGielinorMob*> Near;
		for (TActorIterator<AGielinorMob> It(GetWorld()); It; ++It)
			if (*It != Mob && !It->IsDead() && It->GetHp() > 0 && FVector::Dist2D(It->GetActorLocation(), C) < 500.f) Near.Add(*It);
		for (AGielinorMob* M : Near)
		{
			const int32 D = FMath::Min(M->GetHp(), FMath::FloorToInt(Mh * (0.5f + FMath::FRand() * 0.5f)));
			TMap<FName, float> Xp; Xp.Add(TEXT("magic"), D * 0.6f); GiveXp(Xp);
			M->TakePlayerDamage(D, GetOwner());
		}
	}
	return Extra;
}

void UPlayerCombatComponent::ApplyHex(AGielinorMob* Mob, USpellDef* Spell, int32 Dmg)
{
	if (!Spell || Spell->Hex.IsNone() || !Mob || Mob->IsDead()) return;
	if (Spell->Hex == TEXT("rot")) { Mob->ApplyVenom(FMath::Max(4, FMath::RoundToInt(Spell->MaxHit * 0.2f))); Msg(FText::Format(LOCTEXT("Rot", "The {0} begins to rot."), Mob->GetDisplayName())); }
	else if (Spell->Hex == TEXT("bind")) { Mob->Stun(4); Msg(FText::Format(LOCTEXT("Bind", "Roots of bog-rope bind the {0}!"), Mob->GetDisplayName())); }
	else if (Spell->Hex == TEXT("wither")) { Mob->Wither(33); Msg(FText::Format(LOCTEXT("Wither", "The {0} withers. It hits softer."), Mob->GetDisplayName())); }
	else if (Spell->Hex == TEXT("leech") && Dmg > 0) if (AGielinorPlayerState* S = PS()) S->Health->Heal(Dmg / 2);
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
	if (Special == TEXT("challenge") && !Mob->IsDead()) { Mob->Challenge(4, 10); Msg(FText::Format(LOCTEXT("Challenge", "You shove the {0} back on its heels; it only has eyes for you."), Mob->GetDisplayName())); }
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
