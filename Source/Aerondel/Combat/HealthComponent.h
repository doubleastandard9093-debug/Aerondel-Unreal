#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, int32, Current, int32, Max, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDied, AActor*, Killer);

UCLASS(ClassGroup=(Aerondel), meta=(BlueprintSpawnableComponent))
class AERONDEL_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnHealthChanged OnHealthChanged;
	UPROPERTY(BlueprintAssignable) FOnDied OnDied;

	UFUNCTION(BlueprintCallable, Category="Health") void SetMax(int32 NewMax, bool bRefill);
	UFUNCTION(BlueprintCallable, Category="Health") void ApplyDamage(int32 Amount, AActor* Source);
	UFUNCTION(BlueprintCallable, Category="Health") void Heal(int32 Amount);
	UFUNCTION(BlueprintPure, Category="Health") int32 GetCurrent() const { return Current; }
	/** Save/load: sets current HP (clamped to 1..Max) without events. */
	void SetCurrentRaw(int32 Value) { Current = FMath::Clamp(Value, 1, Max); }
	UFUNCTION(BlueprintPure, Category="Health") int32 GetMax() const { return Max; }
	UFUNCTION(BlueprintPure, Category="Health") bool IsDead() const { return Current <= 0; }

private:
	UPROPERTY(EditAnywhere, Category="Health") int32 Max = 10;
	UPROPERTY(VisibleAnywhere, Category="Health") int32 Current = 10;
};
