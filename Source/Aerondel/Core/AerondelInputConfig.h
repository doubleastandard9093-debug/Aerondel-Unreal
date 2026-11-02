#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "AerondelInputConfig.generated.h"

/**
 * Where the pawns find their Enhanced Input assets (created by Content/Python/create_input_assets.py).
 * Editable in Project Settings > Game > Aerondel Input. Defaults match the script's output paths.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Aerondel Input"))
class AERONDEL_API UAerondelInputConfig : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UAerondelInputConfig();
	// Desktop / gamepad
	UPROPERTY(Config, EditAnywhere, Category="Desktop") TSoftObjectPtr<UInputMappingContext> DesktopContext;
	UPROPERTY(Config, EditAnywhere, Category="Desktop") TSoftObjectPtr<UInputAction> Move;
	UPROPERTY(Config, EditAnywhere, Category="Desktop") TSoftObjectPtr<UInputAction> Look;
	UPROPERTY(Config, EditAnywhere, Category="Desktop") TSoftObjectPtr<UInputAction> OrbitHold;
	UPROPERTY(Config, EditAnywhere, Category="Desktop") TSoftObjectPtr<UInputAction> Zoom;
	UPROPERTY(Config, EditAnywhere, Category="Desktop") TSoftObjectPtr<UInputAction> Click;
	UPROPERTY(Config, EditAnywhere, Category="Desktop") TSoftObjectPtr<UInputAction> Interact;   // gamepad A / E: nearest interactable
	UPROPERTY(Config, EditAnywhere, Category="Desktop") TSoftObjectPtr<UInputAction> Jump;
	UPROPERTY(Config, EditAnywhere, Category="Desktop") TSoftObjectPtr<UInputAction> ToggleInventory;
	// XR
	UPROPERTY(Config, EditAnywhere, Category="XR") TSoftObjectPtr<UInputMappingContext> XRContext;
	UPROPERTY(Config, EditAnywhere, Category="XR") TSoftObjectPtr<UInputAction> XRMove;
	UPROPERTY(Config, EditAnywhere, Category="XR") TSoftObjectPtr<UInputAction> XRTurn;
	UPROPERTY(Config, EditAnywhere, Category="XR") TSoftObjectPtr<UInputAction> XRTeleport;
	UPROPERTY(Config, EditAnywhere, Category="XR") TSoftObjectPtr<UInputAction> XRGrabLeft;
	UPROPERTY(Config, EditAnywhere, Category="XR") TSoftObjectPtr<UInputAction> XRGrabRight;
	UPROPERTY(Config, EditAnywhere, Category="XR") TSoftObjectPtr<UInputAction> XRTriggerLeft;
	UPROPERTY(Config, EditAnywhere, Category="XR") TSoftObjectPtr<UInputAction> XRTriggerRight;
	UPROPERTY(Config, EditAnywhere, Category="XR") TSoftObjectPtr<UInputAction> XRMenu;

	static const UAerondelInputConfig* Get() { return GetDefault<UAerondelInputConfig>(); }
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
