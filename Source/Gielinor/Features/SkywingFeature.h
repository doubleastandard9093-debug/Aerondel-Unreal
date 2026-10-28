#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "SkywingFeature.generated.h"

/** Skywing (TERA round): a free-flying steed, unlocked at Agility 70 plus Exalted with any faction. Flight is the walk
 *  controller with a height lift (cruise 18 m, 6..30 m, 6 m steps), x1.9 speed; diving spends 12 run energy. View distance
 *  drops while flying (cull distance hint for the pawn). Toggle on Y. */
UCLASS()
class GIELINOR_API USkywingFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 AgilityReq = 70;
	static constexpr float CruiseM = 18.f;
	static constexpr float MaxM = 30.f;
	static constexpr float MinM = 6.f;
	static constexpr float StepM = 6.f;
	static constexpr float Speed = 1.9f;
	static constexpr float DiveCost = 12.f;

	virtual FString GetSaveKey() const override { return TEXT("skywing"); }
	virtual void SaveState(FJsonObject& Out) const override { Out.SetBoolField(TEXT("unlocked"), bUnlocked); Out.SetNumberField(TEXT("flights"), Flights); }
	virtual void LoadState(const FJsonObject& In) override { bUnlocked = JBool(In, TEXT("unlocked")); Flights = FMath::Max(0, (int32)JInt(In, TEXT("flights"))); }
	virtual void ResetState() override { bUnlocked = false; Flights = 0; bFlying = false; HeightM = CruiseM; }

	/** Checks Agility + Exalted and unlocks if met. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Skywing") bool CheckUnlock();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Skywing") bool ToggleFlight();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Skywing") void Climb() { HeightM = FMath::Min(MaxM, HeightM + StepM); }
	UFUNCTION(BlueprintCallable, Category="Gielinor|Skywing") bool Dive();
	UFUNCTION(BlueprintPure, Category="Gielinor|Skywing") bool IsFlying() const { return bFlying; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Skywing") bool IsUnlocked() const { return bUnlocked; }
	/** Target height above ground in cm (Unreal units). */
	UFUNCTION(BlueprintPure, Category="Gielinor|Skywing") float HeightCm() const { return bFlying ? HeightM * 100.f : 0.f; }

private:
	bool bUnlocked = false, bFlying = false;
	int32 Flights = 0;
	float HeightM = CruiseM;
};
