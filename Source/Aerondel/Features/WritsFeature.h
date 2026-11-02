#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "WritsFeature.generated.h"

class UMobDef;
class USkillDef;

USTRUCT(BlueprintType)
struct FWrit
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Faction;
	UPROPERTY(BlueprintReadOnly) bool bKill = true;
	UPROPERTY(BlueprintReadOnly) FString Target;
	UPROPERTY(BlueprintReadOnly) int32 N = 1;
	UPROPERTY(BlueprintReadOnly) int32 Got = 0;
	UPROPERTY(BlueprintReadOnly) bool bDone = false;
};

/** Daily Writs (3 per faction, 4 factions) and the Weekly Hearthboard great deed. Hash-seeded rolls by wall-clock
 *  day/week; ignores a clock moved backwards and needs ~6 minutes of play between daily rolls. */
UCLASS()
class AERONDEL_API UWritsFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("writs"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Day = 0; WeekN = 0; LastNow = 0; PlayTicks = 0; Daily.Reset(); bHasWeekly = false; Weekly = FWrit(); Done = 0; WeeklyDone = 0; }
	virtual void OnGameTick() override;

	/** "daily", "weekly" or empty. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Writs") FString MaybeRoll(bool bForce = false);
	UFUNCTION(BlueprintPure, Category="Aerondel|Writs") TArray<FWrit> GetDaily() const { return Daily; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Writs") bool GetWeekly(FWrit& Out) const { Out = Weekly; return bHasWeekly; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Writs") static FString Label(const FWrit& W);
	void Progress(bool bKill, const FString& Target, int32 N = 1);
private:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	UFUNCTION() void HandleXp(USkillDef* Skill, float Amount, float TotalXp, int32 Level);
	void Reward(const FWrit& W, bool bWeekly);
	int64 Day = 0, WeekN = 0, LastNow = 0;
	int32 PlayTicks = 0, Done = 0, WeeklyDone = 0, Ticks = 0;
	TArray<FWrit> Daily;
	FWrit Weekly;
	bool bHasWeekly = false;
};
