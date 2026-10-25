#pragma once
#include "CoreMinimal.h"
#include "Interaction/Station.h"
#include "GielinorFire.generated.h"

class UPointLightComponent;

/**
 * Firemaking: a campfire lit from logs. Works as a Fire station (cooks everything a range does) and burns out
 * after LifetimeSeconds. Matches the web build's lightFire: needs a tinderbox (toolbelt/pack/equipped),
 * not in the town centre, not on another fire or next to a station.
 */
UCLASS()
class GIELINOR_API AGielinorFire : public AStation
{
	GENERATED_BODY()
public:
	AGielinorFire();
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fire") float LifetimeSeconds = 60.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fire") TObjectPtr<UPointLightComponent> Glow;

	/** Lights the logs in the backpack slot at the player's feet. FireClass may be a Blueprint subclass (null = this class). */
	UFUNCTION(BlueprintCallable, Category="Fire") static AGielinorFire* LightFrom(AActor* InInstigator, int32 InventorySlot, TSubclassOf<AGielinorFire> FireClass);
	UFUNCTION(BlueprintPure, Category="Fire") static bool IsLogs(const UItemDef* Item);
};
