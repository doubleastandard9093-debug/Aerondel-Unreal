#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AerondelPropInstances.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** One mesh + one material drawn as hierarchical instances (editor blockout scripts batch repeated props into these). */
UCLASS()
class AERONDEL_API AAerondelPropInstances : public AActor
{
	GENERATED_BODY()
public:
	AAerondelPropInstances();

	UFUNCTION(BlueprintCallable, Category="Aerondel|Blockout")
	void Setup(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCollide);

	/** World-space transforms; returns the instance count afterwards. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Blockout")
	int32 AddInstances(const TArray<FTransform>& Transforms);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aerondel|Blockout")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
};
