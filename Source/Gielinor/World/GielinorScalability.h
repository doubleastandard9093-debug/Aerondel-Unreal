#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GielinorScalability.generated.h"

/** Graphics tiers matching the Unity scaffold: Quest (standalone VR), Low, High, Ultra (Lumen/Nanite/VSM). */
UENUM(BlueprintType)
enum class EGielinorTier : uint8 { Quest, Low, High, Ultra };

UCLASS()
class GIELINOR_API UGielinorScalability : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Applies engine scalability + a few cvars. Android/Quest always resolves to Quest. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Graphics", meta=(WorldContext="WorldContext")) static void ApplyTier(const UObject* WorldContext, EGielinorTier Tier);
	/** Low by default on desktop (matches the web build's "start on Low" rule); Quest on Android. */
	UFUNCTION(BlueprintPure, Category="Gielinor|Graphics") static EGielinorTier DefaultTier();
	/** Tier stored in GameUserSettings.ini ([Gielinor] GraphicsTier); DefaultTier() (Low) if never set. */
	UFUNCTION(BlueprintPure, Category="Gielinor|Graphics") static EGielinorTier GetSavedTier();
	/** Applies the tier and remembers it for next launch. Use this from the settings menu. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Graphics", meta=(WorldContext="WorldContext")) static void SetAndSaveTier(const UObject* WorldContext, EGielinorTier Tier);
	/** Applies GetSavedTier(); called by AGielinorGameMode::BeginPlay. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Graphics", meta=(WorldContext="WorldContext")) static void ApplySavedTier(const UObject* WorldContext);
	/** Nanite stays OFF by default (crash fix 9b07fa6). High/Ultra players may opt in; takes effect after a restart. */
	UFUNCTION(BlueprintPure, Category="Gielinor|Graphics") static bool GetNaniteOptIn();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Graphics") static void SetNaniteOptIn(bool bOn);
};
