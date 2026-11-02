#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AerondelScalability.generated.h"

/** Graphics tiers matching the Unity scaffold: Quest (standalone VR), Low, High, Ultra (Lumen/Nanite/VSM). */
UENUM(BlueprintType)
enum class EAerondelTier : uint8 { Quest, Low, High, Ultra };

/** Native broadcast after ApplyTier, so world dressing (AAerondelEnvironment, AAerondelHighDetail) can follow a settings change live. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAerondelTierApplied, EAerondelTier);

UCLASS()
class AERONDEL_API UAerondelScalability : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Applies engine scalability + a few cvars. Android/Quest always resolves to Quest. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Graphics", meta=(WorldContext="WorldContext")) static void ApplyTier(const UObject* WorldContext, EAerondelTier Tier);
	/** Low by default on desktop (matches the web build's "start on Low" rule); Quest on Android. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Graphics") static EAerondelTier DefaultTier();
	/** Tier stored in GameUserSettings.ini ([Aerondel] GraphicsTier); DefaultTier() (Low) if never set. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Graphics") static EAerondelTier GetSavedTier();
	/** Applies the tier and remembers it for next launch. Use this from the settings menu. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Graphics", meta=(WorldContext="WorldContext")) static void SetAndSaveTier(const UObject* WorldContext, EAerondelTier Tier);
	/** Applies GetSavedTier(); called by AAerondelGameMode::BeginPlay. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Graphics", meta=(WorldContext="WorldContext")) static void ApplySavedTier(const UObject* WorldContext);
	/** Nanite stays OFF by default (crash fix 9b07fa6). High/Ultra players may opt in; takes effect after a restart. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Graphics") static bool GetNaniteOptIn();
	UFUNCTION(BlueprintCallable, Category="Aerondel|Graphics") static void SetNaniteOptIn(bool bOn);
	/** Tier most recently applied by ApplyTier (Low until the first apply). */
	static EAerondelTier GetAppliedTier();
	/** Fires after every ApplyTier. Native only (not a UPROPERTY). */
	static FOnAerondelTierApplied OnTierApplied;
};
