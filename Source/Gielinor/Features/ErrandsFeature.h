#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "ErrandsFeature.generated.h"

USTRUCT(BlueprintType)
struct FErrandDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32 Minutes = 0;
	UPROPERTY(BlueprintReadOnly) FString Skill;
	UPROPERTY(BlueprintReadOnly) int32 Xp = 0;
	UPROPERTY(BlueprintReadOnly) TMap<FString, int32> Items;
	UPROPERTY(BlueprintReadOnly) FName Faction;
	UPROPERTY(BlueprintReadOnly) int32 Rep = 0;
};

USTRUCT(BlueprintType)
struct FErrandRun
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Errand;
	UPROPERTY(BlueprintReadOnly) FName Companion;
	UPROPERTY(BlueprintReadOnly) int64 Start = 0;
	UPROPERTY(BlueprintReadOnly) int64 End = 0;
	UPROPERTY(BlueprintReadOnly) bool bOk = false;
};

/** Errand Ledger: send recruited companions (not the follower) on real-time errands; loot goes to the bank.
 *  Crew skills (mechanics pass): each companion levels the errand skill (1-30), +4% yield per level. */
UCLASS()
class GIELINOR_API UErrandsFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 CrewMax = 30;
	virtual FString GetSaveKey() const override { return TEXT("errands"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Running.Reset(); }

	static const TArray<FErrandDef>& Defs();
	static int32 CrewLevel(int32 Xp) { return FMath::Min(CrewMax, 1 + (int32)FMath::Sqrt(FMath::Max(0, Xp) / 40.f)); }
	static float CrewYield(int32 Level) { return 1.f + 0.04f * (FMath::Max(1, Level) - 1); }

	UFUNCTION(BlueprintPure, Category="Gielinor|Errands") const TArray<FErrandRun>& GetRunning() const { return Running; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Errands") float SuccessChance(FName Companion) const;
	UFUNCTION(BlueprintCallable, Category="Gielinor|Errands") bool Start(FName Errand, FName Companion);
	/** Rewards every finished errand. Returns how many came back. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Errands") int32 CollectFinished();
	UFUNCTION(BlueprintPure, Category="Gielinor|Errands") bool IsAway(FName Companion) const { return Running.ContainsByPredicate([Companion](const FErrandRun& R) { return R.Companion == Companion; }); }

private:
	UPROPERTY() TArray<FErrandRun> Running;
};
