#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PartyFollower.generated.h"

class UStaticMeshComponent; class USkeletalMeshComponent;

/** A party companion walking in formation behind the player (web PARTY_SLOTS). Uses a rigged model from
 *  /Game/Aerondel/Models/Rigged when imported (see Content/Python/import_lite_models.py), else a capsule stand-in. */
UCLASS(NotBlueprintable)
class AERONDEL_API APartyFollower : public AActor
{
	GENERATED_BODY()
public:
	APartyFollower();
	virtual void Tick(float Dt) override;
	void Setup(FName InId, const FString& InName, const FString& Style);
	UPROPERTY(Transient) TWeakObjectPtr<AActor> Leader;
	/** Offset in cm in the leader's local frame (x right, y back). */
	FVector2D Slot = FVector2D::ZeroVector;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Rig;
	FName Id;
	FString DisplayName;
};
