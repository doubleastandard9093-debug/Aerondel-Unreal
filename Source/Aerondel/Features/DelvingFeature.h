#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "DelvingFeature.generated.h"

class AAerondelMob;

USTRUCT(BlueprintType)
struct FDelveDoor
{
	GENERATED_BODY()
	/** "key" (needs <Key> key), "gate" (skill check) or "open" (the link into the guardian's room). */
	UPROPERTY(BlueprintReadOnly) FString Kind;
	UPROPERTY(BlueprintReadOnly) FString Key;
	UPROPERTY(BlueprintReadOnly) FString Skill;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32 Need = 0;
	UPROPERTY(BlueprintReadOnly) bool bOpen = false;
};

/** Delving (web js/delving.js): procedurally assembled floors at the Delver's Descent (Sunscour Dunes). Small/medium/large
 *  floors (5/7/9 rooms, 2/3/4 side rooms), difficulty 1..10 unlocked by Delving level. Gather ore/logs/cavefish, craft tiered
 *  Delve gear at the floor forge, cook at the campfire, find coloured keys in side rooms for locked doors, pass skill gates,
 *  beat the Floor guardian. Tokens buy Delving gear. Floor-bound items are removed when you leave.
 *  Save: {tokens, floors, best, deepest}. */
UCLASS()
class AERONDEL_API UDelvingFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	virtual FString GetSaveKey() const override { return TEXT("delving"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Tokens = Floors = Best = Deepest = 0; }
	virtual void OnGameTick() override;
	virtual void OnPlayerDied() override { if (bInRun) Finish(false); }

	UFUNCTION(BlueprintPure, Category="Aerondel|Delving") int32 Unlocked() const { return FMath::Min(10, 1 + SkillLevel(TEXT("Delving")) / 10); }
	/** Size: small, medium or large. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Delving") FString Enter(FName Size, int32 Difficulty);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Delving") FString Gather(const FString& Skill);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Delving") void StopGather() { GatherSkill.Reset(); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Delving") TArray<FString> GetRecipes() const;
	UFUNCTION(BlueprintCallable, Category="Aerondel|Delving") FString Forge(int32 Index);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Delving") FString Cook();
	/** Pick up the key lying in side room Index. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Delving") FString TakeKey(int32 Index);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Delving") FString OpenDoor(int32 Index);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Delving") void Leave() { Finish(bBossDead); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Delving") TArray<FDelveDoor> GetDoors() const { return Doors; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Delving") TArray<FString> GetKeysOnFloor() const { return FloorKeys; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Delving") TArray<FString> GetShop() const;
	UFUNCTION(BlueprintCallable, Category="Aerondel|Delving") FString Buy(int32 Index);
	UFUNCTION(BlueprintPure, Category="Aerondel|Delving") int32 GetTokens() const { return Tokens; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Delving") bool IsInRun() const { return bInRun; }

	/** Delver's Descent (web metres). */
	static FVector2D Hub() { return FVector2D(330.f, 106.f); }
private:
	void SpawnFoe(bool bBoss, FVector2D Pos);
	void Opened();
	void Finish(bool bWin);
	int32 Tokens = 0, Floors = 0, Best = 0, Deepest = 0;
	bool bInRun = false, bBossDead = false;
	int32 Diff = 1, Level = 1, Tier = 1, Rooms = 5, RunTicks = 0, TickN = 0, GatherNext = 0, Reached = 0;
	FString Reward, GatherSkill;
	TArray<FDelveDoor> Doors;
	TArray<FString> FloorKeys;
	TArray<TWeakObjectPtr<AAerondelMob>> Live;
	TWeakObjectPtr<AAerondelMob> Boss;
};
