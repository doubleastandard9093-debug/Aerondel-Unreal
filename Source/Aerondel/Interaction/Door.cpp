#include "Interaction/Door.h"
#include "Components/StaticMeshComponent.h"

ADoor::ADoor()
{
	PrimaryActorTick.bCanEverTick = true;
	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);
	Mesh->SetupAttachment(Hinge);
	Range = 220.f;
}

void ADoor::BeginPlay()
{
	Super::BeginPlay();
	bOpen = bStartOpen; Yaw = bOpen ? OpenAngle : 0.f;
	Hinge->SetRelativeRotation(FRotator(0, Yaw, 0));
}

FText ADoor::GetInteractLabel() const { return bOpen ? NSLOCTEXT("Aerondel", "Close", "Close Door") : NSLOCTEXT("Aerondel", "Open", "Open Door"); }

FVector ADoor::GetInteractPoint() const { return Mesh->Bounds.Origin; }

void ADoor::Interact(AActor* InInstigator)
{
	bOpen = !bOpen;
	if (bOpen && InInstigator)
	{
		// Swing away from whoever opened it.
		const FVector Local = GetActorTransform().InverseTransformPosition(InInstigator->GetActorLocation());
		Sign = Local.Y > 0 ? -1.f : 1.f;
	}
}

void ADoor::Tick(float Dt)
{
	Super::Tick(Dt);
	const float Target = bOpen ? OpenAngle * Sign : 0.f;
	if (!FMath::IsNearlyEqual(Yaw, Target, 0.1f))
	{
		Yaw = FMath::FInterpConstantTo(Yaw, Target, Dt, SwingSpeed);
		Hinge->SetRelativeRotation(FRotator(0, Yaw, 0));
	}
}
