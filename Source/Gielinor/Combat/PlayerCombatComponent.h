#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerCombatComponent.generated.h"

class AGielinorMob; class AGielinorPlayerState;

UENUM(BlueprintType)
enum class ECombatStyle : uint8 { Accurate, Aggressive, Defensive, Controlled };

/**
 * Lives on BOTH pawns. Desktop: click a mob -> SetTarget -> walk into range -> auto-attack every AttackSeconds.
 * VR: the swing sensor calls AGielinorMob::ReceivePlayerHit directly (cooldown enforced here via CanSwing()).
 */
UCLASS(ClassGroup=(Gielinor), meta=(BlueprintSpawnableComponent))
class GIELINOR_API UPlayerCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPlayerCombatComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") ECombatStyle Style = ECombatStyle::Accurate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float AttackSeconds = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float MeleeRange = 180.f;
	/** Desktop pawns walk to the target; VR pawns don't auto-move (comfort) - set false there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") bool bAutoApproach = true;

	UFUNCTION(BlueprintCallable, Category="Combat") void SetTarget(AGielinorMob* Mob);
	UFUNCTION(BlueprintCallable, Category="Combat") void ClearTarget();
	UFUNCTION(BlueprintPure, Category="Combat") AGielinorMob* GetTarget() const { return Target.Get(); }
	UFUNCTION(BlueprintCallable, Category="Combat") bool ConsumeSwing();

	void OnAttackedBy(AGielinorMob* Mob) { if (!Target.IsValid()) SetTarget(Mob); }
	void AwardXp(AGielinorPlayerState* PS, int32 Damage) const;

	virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* Fn) override;
private:
	TWeakObjectPtr<AGielinorMob> Target;
	float Cooldown = 0.f;
};
