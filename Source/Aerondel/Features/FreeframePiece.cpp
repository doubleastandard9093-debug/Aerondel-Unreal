#include "Features/FreeframePiece.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

AFreeframePiece::AFreeframePiece()
{
	PrimaryActorTick.bCanEverTick = false;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Body->SetStaticMesh(Cube.Object);
}

void AFreeframePiece::Setup(const FLinearColor& Color, float HeightCm, bool bBurning)
{
	const float XY = 1.45f * 100.f / 100.f; // grid is 1.5 m; cube is 100 uu
	Body->SetRelativeScale3D(FVector(1.42f, 1.42f, FMath::Max(0.3f, HeightCm / 100.f)));
	FLinearColor C = bBurning ? FLinearColor(0.76f, 0.29f, 0.07f) : Color;
	if (UMaterialInstanceDynamic* Mid = Body->CreateAndSetMaterialInstanceDynamic(0))
		Mid->SetVectorParameterValue(TEXT("Color"), C);
	Body->SetCanEverAffectNavigation(false);
}
