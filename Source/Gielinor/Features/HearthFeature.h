#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "HearthFeature.generated.h"

class UMobDef;

/** Hearthstead (Phase 5): buy a deed, build 4 rooms, craft furniture; rooms give lasting perks
 *  (workshop/garden XP, boss trophies for boss damage, companion quarters for errands and daily bond). */
UCLASS()
class GIELINOR_API UHearthFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 DeedCost = 25000;
	static constexpr int64 HarvestMs = 12ll * 3600 * 1000;
	static constexpr int64 BondDayMs = 20ll * 3600 * 1000;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("house"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { bDeed = false; Rooms.Reset(); Furniture.Reset(); Trophies.Reset(); HarvestAt = BondAt = 0; }
	virtual float XpBonus(FName SkillId) const override;

	UFUNCTION(BlueprintPure, Category="Gielinor|Hearth") bool HasDeed() const { return bDeed; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Hearth") bool HasRoom(FName Room) const { return bDeed && Rooms.Contains(Room.ToString()); }
	UFUNCTION(BlueprintPure, Category="Gielinor|Hearth") bool HasFurniture(FName F) const { return Furniture.Contains(F.ToString()); }
	UFUNCTION(BlueprintCallable, Category="Gielinor|Hearth") bool BuyDeed();
	/** workshop | garden | trophy | quarters */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Hearth") bool BuildRoom(FName Room);
	/** bench, anvil, planter, trough, plinth, banners, bunks, stove */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Hearth") bool BuildFurniture(FName F);
	UFUNCTION(BlueprintPure, Category="Gielinor|Hearth") float BossDamageBonus() const;
	UFUNCTION(BlueprintPure, Category="Gielinor|Hearth") float ErrandBonus() const;
	/** Garden herbs (0 if not ready). Gives them to the player. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Hearth") int32 TakeHarvest();
	/** Companion quarters: daily bond for every recruited companion. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Hearth") int32 TakeBondDay();
	UFUNCTION(BlueprintPure, Category="Gielinor|Hearth") TArray<FString> GetTrophies() const { return Trophies; }

private:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	bool bDeed = false;
	TArray<FString> Rooms, Furniture, Trophies;
	int64 HarvestAt = 0, BondAt = 0;
};
