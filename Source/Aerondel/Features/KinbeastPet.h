#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KinbeastPet.generated.h"

class UStaticMeshComponent;

/** A small walking placeholder for a sent-out Kinbeast (Engine sphere, no collision, no shadows). */
UCLASS(NotBlueprintable)
class AERONDEL_API AKinbeastPet : public AActor
{
	GENERATED_BODY()
public:
	AKinbeastPet();
	virtual void Tick(float Dt) override;
	void Setup(AActor* InLeader, const FLinearColor& Color);
	UPROPERTY(Transient) TWeakObjectPtr<AActor> Leader;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
};
