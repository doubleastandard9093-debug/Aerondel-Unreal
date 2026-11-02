#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/AerondelRules.h"
#include "PlayerCombatComponent.generated.h"

class AAerondelMob; class AAerondelPlayerState; class USpellDef;

/**
 * Lives on BOTH pawns. Desktop: click a mob -> SetTarget -> walk into range -> auto-attack on the weapon's tick speed.
 * Melee, ranged (ammo, Double Shot, multi-arrow) and magic (autocast) like the web build, plus queued abilities
 * (power, puncture, bash, snipe, volley) and Berserk. VR: swings call PlayerAttack with a 0..1 power (cooldown via ConsumeSwing).
 * The attack style index, autocast spell and ability state live on UPlayerStatsComponent so they are saved.
 */
UCLASS(ClassGroup=(Aerondel), meta=(BlueprintSpawnableComponent))
class AERONDEL_API UPlayerCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPlayerCombatComponent();
	/** Desktop pawns walk to the target; VR pawns don't auto-move (comfort) - set false there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") bool bAutoApproach = true;
	/** Extra reach (cm) added to the tile range for capsule sizes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float ReachSlackCm = 60.f;

	UFUNCTION(BlueprintCallable, Category="Combat") void SetTarget(AAerondelMob* Mob);
	UFUNCTION(BlueprintCallable, Category="Combat") void ClearTarget();
	UFUNCTION(BlueprintPure, Category="Combat") AAerondelMob* GetTarget() const { return Target.Get(); }
	/** VR: true (and starts the cooldown) if a swing may land now. */
	UFUNCTION(BlueprintCallable, Category="Combat") bool ConsumeSwing();
	/** Uses an ability from the quick bar (fortify/rage/rejuv fire now; others queue for the next attack). */
	UFUNCTION(BlueprintCallable, Category="Combat") FText UseAbility(FName AbilityId);
	UFUNCTION(BlueprintPure, Category="Combat") float AttackRangeCm() const;

	/** One attack on Mob (melee or ranged by weapon, magic when autocasting). Power scales damage (VR swings). */
	void PlayerAttack(AAerondelMob* Mob, float Power = 1.f);
	/** Riposte: a free hit with normal xp. */
	void BonusHit(AAerondelMob* Mob, const FText& Label);
	/** Mob hit us: auto-retaliate when idle. */
	void OnAttackedBy(AAerondelMob* Mob);

	virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* Fn) override;
private:
	AAerondelPlayerState* PS() const;
	void GiveXp(const TMap<FName, float>& Xp) const;
	void Msg(const FText& T) const;
	bool CastAt(AAerondelMob* Mob, USpellDef* Spell);
	/** Magic kit: a queued magic ability (bolts, Surge, Storm) rides on this cast. Returns extra damage. */
	int32 MagicKit(AAerondelMob* Mob, USpellDef* Spell);
	/** Hexcraft effects: rot, bind, wither, leech. */
	void ApplyHex(AAerondelMob* Mob, USpellDef* Spell, int32 Dmg);
	TWeakObjectPtr<AAerondelMob> Target;
	float Cooldown = 0.f;
};
