#include "World/AerondelPropInstances.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/CollisionProfile.h"

AAerondelPropInstances::AAerondelPropInstances()
{
	PrimaryActorTick.bCanEverTick = false;
	Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Instances"));
	RootComponent = Instances;
	Instances->SetMobility(EComponentMobility::Static);
}

void AAerondelPropInstances::Setup(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCollide)
{
	Instances->SetStaticMesh(Mesh);
	if (Material) Instances->SetMaterial(0, Material);
	Instances->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Instances->SetCastShadow(true);
}

int32 AAerondelPropInstances::AddInstances(const TArray<FTransform>& Transforms)
{
	if (Transforms.Num() > 0) Instances->AddInstances(Transforms, false, true);
	return Instances->GetInstanceCount();
}
