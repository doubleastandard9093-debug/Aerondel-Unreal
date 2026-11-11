#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AshkinSpirit.generated.h"

class UStaticMeshComponent;

/** A small hovering placeholder for a summoned Ashkin (Engine sphere, no collision, no shadows). */
UCLASS(NotBlueprintable)
class AERONDEL_API AAshkinSpirit : public AActor
{
	GENERATED_BODY()
public:
	AAshkinSpirit();
	virtual void Tick(float Dt) override;
	void Setup(AActor* InLeader, const FString& Style);
	UPROPERTY(Transient) TWeakObjectPtr<AActor> Leader;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
};
