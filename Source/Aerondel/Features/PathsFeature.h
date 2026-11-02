#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "PathsFeature.generated.h"

class UMobDef;

USTRUCT(BlueprintType)
struct FPathFx
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) float Red = 0.f;
	UPROPERTY(BlueprintReadOnly) float Dmg = 0.f;
	UPROPERTY(BlueprintReadOnly) float Acc = 0.f;
	UPROPERTY(BlueprintReadOnly) float Regen = 0.f;
	UPROPERTY(BlueprintReadOnly) float KillHeal = 0.f;
};

/** Combat Paths: at combat 30 pick Bulwark (tank), Mender (healer) or Striker (damage); talent rows every 10 combat levels (1 of 3). */
UCLASS()
class AERONDEL_API UPathsFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 PathLevel = 30;
	static constexpr int32 NumRows = 7;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("path"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { PathId = NAME_None; Talents.Init(-1, NumRows); Switches = 0; RegenAcc = 0.f; }
	virtual float DamageTakenMult(const UMobDef* Source, float HpFrac) const override { return 1.f - Fx().Red; }
	virtual float DamageDealtBonus(const UMobDef* Target) const override { return Fx().Dmg; }
	virtual void OnGameTick() override;

	/** bulwark | mender | striker. Free to switch (at the lectern or Pathwarden Idris); talents reset. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Paths") FString Choose(FName NewPath);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Paths") FString PickTalent(int32 Row, int32 Choice);
	UFUNCTION(BlueprintPure, Category="Aerondel|Paths") int32 RowsOpen() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Paths") FPathFx Fx() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Paths") FName GetPath() const { return PathId; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Paths") TArray<FName> PathSkills() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Paths") static FString TalentName(int32 Row, int32 Choice);
private:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	FName PathId;
	TArray<int32> Talents;
	int32 Switches = 0;
	float RegenAcc = 0.f;
};
