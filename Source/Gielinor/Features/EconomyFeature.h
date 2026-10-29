#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "EconomyFeature.generated.h"

class UItemDef; struct FShopStock; class AGielinorPlayerState;

/** Economy sinks (web economy.js): finite shop stock that refills over 200 ticks, buy prices that climb up to +60% as a line
 *  sells out (never below 115% of the sell-back price), wear on top-tier gear (+0.25% per hit taken; fully worn keeps 70% of its
 *  bonuses; repair at any shop for value * 0.002 * wear%), and a death fee of 5% of carried coins (max 50,000). Tools are
 *  unlimited and never climb. */
UCLASS()
class GIELINOR_API UEconomyFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 RestockTicks = 200;
	static constexpr float PriceClimb = 0.6f, ArbMargin = 1.15f, WearPerHit = 0.25f, WornMult = 0.7f, RepairRate = 0.002f, DeathFee = 0.05f;
	static constexpr int32 DeathFeeCap = 50000;

	virtual FString GetSaveKey() const override { return TEXT("economy"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Bought.Reset(); Wear.Reset(); }
	virtual void OnGameTick() override;
	virtual void OnPlayerDied() override;

	static int32 StockCap(int32 Price);
	static bool IsTopTier(const UItemDef* Item);
	/** Units left in a shop line (MAX_int32 for tools). */
	int32 StockLeft(FName ShopId, const FShopStock& Entry, int32 BasePrice) const;
	/** Price of the next unit: Base climbs with what you've bought; never under SellBack * 1.15. */
	int32 UnitPrice(FName ShopId, const FShopStock& Entry, int32 Base, int32 SellBack) const;
	void NoteBought(FName ShopId, const UItemDef* Item, int32 N);
	/** A hit landed on the player: wears every equipped top-tier item. */
	void OnPlayerHit();
	/** 1.0, or 0.7 for fully worn gear. */
	float WearMult(const UItemDef* Item) const;

	UFUNCTION(BlueprintPure, Category="Gielinor|Economy") float WearOf(const UItemDef* Item) const;
	UFUNCTION(BlueprintPure, Category="Gielinor|Economy") int32 RepairCost() const;
	UFUNCTION(BlueprintCallable, Category="Gielinor|Economy") FString RepairAll();

	/** Static lookup for code without a feature handle (rules, shop counters). */
	static UEconomyFeature* Get(const UObject* WorldContext);

private:
	static FString Key(FName ShopId, const UItemDef* Item);
	/** "shop:itemId" -> units bought (fractional while restocking). */
	TMap<FString, float> Bought;
	TMap<FName, float> Wear;
};
