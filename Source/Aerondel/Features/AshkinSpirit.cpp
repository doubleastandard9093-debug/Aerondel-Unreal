#include "Features/AshkinSpirit.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

AAshkinSpirit::AAshkinSpirit()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) Body->SetStaticMesh(Sphere.Object);
	Body->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.36f));
}

void AAshkinSpirit::Setup(AActor* InLeader, const FString& Style)
{
	Leader = InLeader;
	FLinearColor Col(0.85f, 0.35f, 0.12f); // Ember ash
	if (Style == TEXT("melee")) Col = FLinearColor(0.45f, 0.55f, 0.85f);
	else if (Style == TEXT("ranged")) Col = FLinearColor(0.35f, 0.7f, 0.35f);
	if (UMaterialInstanceDynamic* Mid = Body->CreateAndSetMaterialInstanceDynamic(0))
		Mid->SetVectorParameterValue(TEXT("Color"), Col);
	Body->SetCanEverAffectNavigation(false);
	if (InLeader) SetActorLocation(InLeader->GetActorLocation() + InLeader->GetActorRightVector() * -80.f + FVector(0.f, 0.f, 110.f));
}

void AAshkinSpirit::Tick(float Dt)
{
	Super::Tick(Dt);
	AActor* L = Leader.Get();
	if (!L) return;
	const FVector Goal = L->GetActorLocation() + L->GetActorRightVector() * -80.f + L->GetActorForwardVector() * -20.f + FVector(0.f, 0.f, 110.f);
	const FVector P = GetActorLocation();
	const FVector To = Goal - P;
	if (To.Size() > 2500.f) { SetActorLocation(Goal); return; }
	SetActorLocation(FMath::VInterpTo(P, Goal, Dt, 6.f));
}
