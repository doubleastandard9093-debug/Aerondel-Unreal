#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SailingVisuals.generated.h"

class UBoxComponent; class UStaticMeshComponent; class UInstancedStaticMeshComponent; class UMaterialInterface; class USceneComponent;

/** One procedural ship built from engine basic shapes (original, no imported art): hull, bow, stern castle, mast(s), sails, guns. */
USTRUCT()
struct FShipParts
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	int32 BuiltKind = -1;
};

/** Draws USailingFeature's sim: your ship (hull tier 1-3), Drowned Crew ships, the cannonball pool and chart X marks.
 *  Also carries a walkable deck box under the player while at sea, so the pawn stays in MOVE_Walking.
 *  Spawned on demand by USailingFeature. Low/Quest tier: hulls + masts only (no sail cloth, no wake), 10-ball pool. */
UCLASS()
class GIELINOR_API ASailingVisuals : public AActor
{
	GENERATED_BODY()
public:
	ASailingVisuals();
	virtual void Tick(float Dt) override;
	/** Deck top height above sea level, cm. USailingFeature stands the pawn here. */
	static constexpr float DeckTopCm = 130.f;
protected:
	virtual void BeginPlay() override;
private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Deck;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Balls;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> ChartMarks;
	UPROPERTY() FShipParts Mine;
	UPROPERTY() TArray<FShipParts> Foes;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMat;
	bool bLow = true;
	void BuildShip(FShipParts& S, int32 Kind, bool bEnemy);
	UStaticMeshComponent* Part(FShipParts& S, const TCHAR* Shape, const FVector& Loc, const FVector& Scale, const FLinearColor& Color, const FRotator& Rot = FRotator::ZeroRotator);
	static void PlaceShip(FShipParts& S, float X, float Z, float Yaw, bool bVisible);
};
