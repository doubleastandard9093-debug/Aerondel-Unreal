#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AerondelFloatText.generated.h"

class UTextRenderComponent;

/**
 * Short-lived rising combat number (hit, miss/0, or a green heal). Cheap TextRender, capped on Low/Quest.
 */
UCLASS()
class AERONDEL_API AAerondelFloatText : public AActor
{
	GENERATED_BODY()
public:
	AAerondelFloatText();
	virtual void Tick(float Dt) override;

	/** Damage (0 = miss) or a heal (bHeal) above WorldLocation. */
	static void Show(const UObject* WorldContext, const FVector& WorldLocation, int32 Amount, bool bHeal = false);
	/** Convenience: spawn above an actor's capsule / bounds. */
	static void ShowOn(const AActor* Target, int32 Amount, bool bHeal = false);

private:
	UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
	float Age = 0.f;
	float Rise = 90.f;
};
