#pragma once
#include "CoreMinimal.h"
#include "Interaction/Station.h"
#include "AerondelFire.generated.h"

class UPointLightComponent;

/**
 * Firemaking: a campfire lit from logs. Works as a Fire station (cooks everything a range does) and burns out
 * after LifetimeSeconds. Matches the web build's lightFire: needs a tinderbox (toolbelt/pack/equipped),
 * not in the town centre, not on another fire or next to a station.
 */
UCLASS()
class AERONDEL_API AAerondelFire : public AStation
{
	GENERATED_BODY()
public:
	AAerondelFire();
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fire") float LifetimeSeconds = 60.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fire") TObjectPtr<UPointLightComponent> Glow;

	/** Lights the logs in the backpack slot at the player's feet. FireClass may be a Blueprint subclass (null = this class). */
	UFUNCTION(BlueprintCallable, Category="Fire") static AAerondelFire* LightFrom(AActor* InInstigator, int32 InventorySlot, TSubclassOf<AAerondelFire> FireClass);
	UFUNCTION(BlueprintPure, Category="Fire") static bool IsLogs(const UItemDef* Item);
};
