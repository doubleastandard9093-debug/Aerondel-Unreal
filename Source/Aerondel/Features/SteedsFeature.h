#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "SteedsFeature.generated.h"

class USkillDef;

/** Shiftsteeds (Phase 2): Fernback Stag (Agility 10, x1.45) and Ironbristle Boar (Agility 30, x1.3) with Bulwark form
 *  (6 s, -30% damage taken, short charge, 20 s cooldown). Roads add x1.15. Granted automatically on Agility level-ups.
 *  The pawn reads SpeedMultiplier() for its max walk speed; mesh is a placeholder until art lands. */
UCLASS()
class AERONDEL_API USteedsFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	static constexpr float RoadBonus = 1.15f;
	static constexpr int64 BulwarkMs = 6000;
	static constexpr int64 BulwarkCdMs = 20000;
	static constexpr float BulwarkReduce = 0.3f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("steeds"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Owned.Reset(); Active = NAME_None; bMounted = false; BulwarkUntil = BulwarkReadyAt = 0; }

	UFUNCTION(BlueprintCallable, Category="Aerondel|Steeds") int32 GrantForAgility(int32 AgilityLevel);
	UFUNCTION(BlueprintPure, Category="Aerondel|Steeds") bool Owns(FName Steed) const { return Owned.Contains(Steed); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Steeds") TArray<FName> GetOwned() const { return Owned; }
	UFUNCTION(BlueprintCallable, Category="Aerondel|Steeds") bool Summon(FName Steed);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Steeds") void Dismount() { bMounted = false; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Steeds") bool IsMounted() const { return bMounted; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Steeds") float SpeedMultiplier(bool bOnRoad) const;
	UFUNCTION(BlueprintCallable, Category="Aerondel|Steeds") bool ShiftBulwark();
	/** Damage-taken multiplier (0.7 in Bulwark form). */
	UFUNCTION(BlueprintPure, Category="Aerondel|Steeds") float IncomingMultiplier() const { return NowMs() < BulwarkUntil ? 1.f - BulwarkReduce : 1.f; }

private:
	UFUNCTION() void HandleLevelUp(USkillDef* Skill, int32 NewLevel);
	TArray<FName> Owned;
	FName Active;
	bool bMounted = false;
	int64 BulwarkUntil = 0, BulwarkReadyAt = 0;
};
