#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "Engine/TimerHandle.h"
#include "StillwatchFeature.generated.h"

UENUM(BlueprintType)
enum class EWeakSpot : uint8 { Head, Legs, Arms };

/** Stillwatch (Outer Worlds round): a focus meter filled by Flow finishers (+35) and perfect dodges (+25). When full,
 *  trigger it to slow the world to 35% for 4 s (2 s vs bosses) and aim a weak spot: head = crit (+60% dmg, +20% acc),
 *  legs = slow its attacks 3 ticks, arms = weaken its blows 10 ticks. Uses global time dilation; the player pawn is
 *  compensated with CustomTimeDilation so it keeps full speed. */
UCLASS()
class GIELINOR_API UStillwatchFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	static constexpr float Max = 100.f;
	static constexpr float FocusFinisher = 35.f;
	static constexpr float FocusPerfect = 25.f;
	static constexpr float Scale = 0.35f;
	static constexpr float Seconds = 4.f;
	static constexpr float BossSeconds = 2.f;

	virtual FString GetSaveKey() const override { return TEXT("still"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Meter = 0.f; Uses = 0; Spot = EWeakSpot::Head; }

	/** Returns true when the meter is full. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Stillwatch") bool AddFocus(float N) { Meter = FMath::Min(Max, Meter + N); return Meter >= Max; }
	UFUNCTION(BlueprintCallable, Category="Gielinor|Stillwatch") bool Trigger(bool bVsBoss);
	UFUNCTION(BlueprintCallable, Category="Gielinor|Stillwatch") void SetSpot(EWeakSpot S) { Spot = S; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Stillwatch") EWeakSpot GetSpot() const { return Spot; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Stillwatch") float GetMeter() const { return Meter; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Stillwatch") bool IsActive() const { return bActive; }
	/** Damage/accuracy mods for a hit landed during Stillwatch (head only). */
	UFUNCTION(BlueprintPure, Category="Gielinor|Stillwatch") float SpotDamageBonus() const { return bActive && Spot == EWeakSpot::Head ? 0.6f : 0.f; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Stillwatch") float SpotAccuracyBonus() const { return bActive && Spot == EWeakSpot::Head ? 0.2f : 0.f; }
	/** Debuff ticks to put on the foe (legs: slow, arms: weaken). */
	UFUNCTION(BlueprintPure, Category="Gielinor|Stillwatch") int32 SpotDebuffTicks() const { return !bActive ? 0 : Spot == EWeakSpot::Legs ? 3 : Spot == EWeakSpot::Arms ? 10 : 0; }

private:
	void End();
	float Meter = 0.f;
	int32 Uses = 0;
	EWeakSpot Spot = EWeakSpot::Head;
	bool bActive = false;
	FTimerHandle EndTimer;
};
