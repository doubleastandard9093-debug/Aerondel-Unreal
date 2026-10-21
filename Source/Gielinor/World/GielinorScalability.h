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
};
