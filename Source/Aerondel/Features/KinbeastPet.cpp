#include "Features/KinbeastPet.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

AKinbeastPet::AKinbeastPet()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) Body->SetStaticMesh(Sphere.Object);
	Body->SetRelativeScale3D(FVector(0.38f, 0.42f, 0.32f));
}

void AKinbeastPet::Setup(AActor* InLeader, const FLinearColor& Color)
{
	Leader = InLeader;
	if (UMaterialInstanceDynamic* Mid = Body->CreateAndSetMaterialInstanceDynamic(0))
		Mid->SetVectorParameterValue(TEXT("Color"), Color);
	Body->SetCanEverAffectNavigation(false);
	if (InLeader) SetActorLocation(InLeader->GetActorLocation() + InLeader->GetActorRightVector() * 80.f + InLeader->GetActorForwardVector() * -60.f + FVector(0.f, 0.f, 30.f));
}

void AKinbeastPet::Tick(float Dt)
{
	Super::Tick(Dt);
	AActor* L = Leader.Get();
	if (!L) return;
	const FVector Goal = L->GetActorLocation() + L->GetActorRightVector() * 80.f + L->GetActorForwardVector() * -60.f + FVector(0.f, 0.f, 30.f);
	const FVector P = GetActorLocation();
	const FVector To = Goal - P;
	if (To.Size() > 2500.f) { SetActorLocation(Goal); return; }
	SetActorLocation(FMath::VInterpTo(P, Goal, Dt, 5.f));
}
