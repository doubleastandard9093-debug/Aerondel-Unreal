#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FreeframePiece.generated.h"

class UStaticMeshComponent;

/** A simple box stand-in for one Freeframe piece on Claimridge (Engine cube). */
UCLASS(NotBlueprintable)
class AERONDEL_API AFreeframePiece : public AActor
{
	GENERATED_BODY()
public:
	AFreeframePiece();
	void Setup(const FLinearColor& Color, float HeightCm, bool bBurning);
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	FString PieceId;
};
