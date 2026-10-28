#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "TasksFeature.generated.h"

class UMobDef;

/** Hunt tasks (Slayer-style) from Huntmaster Corvane: assigned kills, points with streak multipliers,
 *  skips/blocks/extends, a points shop, and Drowned Crew standing. */
UCLASS()
class GIELINOR_API UTasksFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("tasks"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { CurMob = NAME_None; CurN = 0; CurGot = 0; Points = 0; Streak = 0; Done = 0; Blocked.Reset(); Extended.Reset(); Unlocks.Reset(); }
	virtual float DamageDealtBonus(const UMobDef* Target) const override;

	UFUNCTION(BlueprintCallable, Category="Gielinor|Tasks") FString Assign();
	/** Skip for 30 points (no streak loss). */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Tasks") FString Skip();
	/** Block the current monster for 100 points (max 4). */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Tasks") FString Block();
	/** Extend tasks of the current monster (x1.5 length, x1.3 points) for 75 points. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Tasks") FString Extend();
	/** insight | ring | fire | lamp | reefward | reefbane | saltring */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Tasks") FString Buy(FName ShopId);
	UFUNCTION(BlueprintPure, Category="Gielinor|Tasks") FString CurrentLabel() const;
	UFUNCTION(BlueprintPure, Category="Gielinor|Tasks") int32 GetPoints() const { return Points; }
	static bool IsDrowned(const UMobDef* M);
private:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	int32 PointsFor(const UMobDef* M) const;
	FName CurMob;
	int32 CurN = 0, CurGot = 0, Points = 0, Streak = 0, Done = 0;
	TArray<FName> Blocked, Extended;
	TArray<FString> Unlocks;
};
