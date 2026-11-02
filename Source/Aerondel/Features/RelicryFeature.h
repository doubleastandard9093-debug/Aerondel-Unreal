#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "RelicryFeature.generated.h"

class UMobDef; class AAerondelMob;

USTRUCT(BlueprintType)
struct FRelicGate
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Skill;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32 Need = 1;
	UPROPERTY(BlueprintReadOnly) bool bOpen = false;
};

/** Relicry (web js/relicry.js): six dig sites (3 trenches, a restoration bench, a vault), restoration, collections, reforging,
 *  and the relic dungeon run behind each vault: 6 skill gates in order, sentinels, the Relic colossus and a reliquary chest.
 *  The run is staged at the vault courtyard (no separate instance map is required). Save: {coll, runs, best, seen}. */
UCLASS()
class AERONDEL_API URelicryFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 NumGates = 6;
	virtual FString GetSaveKey() const override { return TEXT("relicry"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override;
	virtual void OnGameTick() override;
	virtual void OnPlayerDied() override { if (bInRun) Finish(false); }

	UFUNCTION(BlueprintPure, Category="Aerondel|Relicry") TArray<FString> GetSites() const;
	/** Nearest site within 12 m of the player, or -1. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Relicry") int32 NearestSite() const;
	UFUNCTION(BlueprintCallable, Category="Aerondel|Relicry") FString StartDig(int32 Site);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Relicry") void StopDig() { DigSite = -1; }
	UFUNCTION(BlueprintCallable, Category="Aerondel|Relicry") FString Restore(int32 Site, int32 Art);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Relicry") FString TurnIn(int32 Site);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Relicry") FString Reforge(const FString& Gear);
	UFUNCTION(BlueprintPure, Category="Aerondel|Relicry") int32 DungeonLevel(int32 Site) const;
	UFUNCTION(BlueprintCallable, Category="Aerondel|Relicry") FString EnterVault(int32 Site);
	/** Next closed gate: a Relic key opens any gate; fight gates spawn a Vault warden; hp gates cost 10% max HP. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Relicry") FString TryGate(int32 Index, bool bUseKey = false);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Relicry") FString OpenChest();
	UFUNCTION(BlueprintCallable, Category="Aerondel|Relicry") void Leave() { Finish(bBossDead); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Relicry") TArray<FRelicGate> GetGates() const { return Gates; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Relicry") bool IsInRun() const { return bInRun; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Relicry") int32 GetCollections(const FString& Site) const { return Coll.FindRef(Site); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Relicry") int32 GetRuns() const { return Runs; }

	static FString DamagedName(const FString& Art);
private:
	int32 Lvl() const { return SkillLevel(TEXT("Relicry")); }
	int32 MattockTier(FString& OutName) const;
	void SpawnScaled(const FString& Type, FVector2D Pos, bool bBoss);
	void Finish(bool bWin);

	TMap<FString, int32> Coll, Seen;
	int32 Runs = 0, Best = 0;
	int32 DigSite = -1, DigNext = 0, TickN = 0;
	bool bInRun = false, bBossDead = false, bChest = false;
	int32 RunSite = -1, RunLevel = 1, RunTicks = 0;
	TArray<FRelicGate> Gates;
	TArray<TWeakObjectPtr<AAerondelMob>> Live;
	TWeakObjectPtr<AAerondelMob> Boss, Warden;
};
