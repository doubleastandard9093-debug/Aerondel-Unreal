#include "Interaction/GielinorFire.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Pawn.h"
#include "Core/GielinorPlayerState.h"
#include "Core/GielinorEvents.h"
#include "Inventory/InventoryComponent.h"
#include "Rules/GielinorRules.h"
#include "Data/ItemDef.h"

#define LOCTEXT_NAMESPACE "GielinorFire"

AGielinorFire::AGielinorFire()
{
	StationType = EStationType::Fire;
	DisplayName = LOCTEXT("Fire", "Fire");
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetRelativeLocation(FVector(0, 0, 60));
	Glow->SetLightColor(FLinearColor(1.f, 0.55f, 0.15f));
	Glow->SetIntensity(3000.f);
	Glow->SetCastShadows(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (Mesh && Cone.Succeeded())
	{
		Mesh->SetStaticMesh(Cone.Object);
		Mesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.8f));
		Mesh->SetRelativeLocation(FVector(0, 0, 40));
	}
	if (Mesh) Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // lit at the player's feet; clicks/VR still hit it
}

void AGielinorFire::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(LifetimeSeconds);
}

bool AGielinorFire::IsLogs(const UItemDef* Item)
{
	return Item && (Item->Id == TEXT("logs") || Item->Id.ToString().EndsWith(TEXT("_logs")));
}

AGielinorFire* AGielinorFire::LightFrom(AActor* InInstigator, int32 InventorySlot, TSubclassOf<AGielinorFire> FireClass)
{
	AGielinorPlayerState* S = AGielinorPlayerState::FromActor(InInstigator);
	APawn* Pawn = S ? S->GetPawn() : nullptr;
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!World) return nullptr;
	UGielinorEvents* Ev = UGielinorEvents::Get(Pawn);
	const FItemStack Stack = S->Inventory->GetSlot(InventorySlot);
	if (!IsLogs(Stack.Item)) return nullptr;
	if (!UGielinorRules::OwnsToolType(S, EToolType::Tinderbox))
	{
		if (Ev) Ev->Message(LOCTEXT("NoTinder", "You need a tinderbox on your toolbelt."));
		return nullptr;
	}
	FVector Loc = Pawn->GetActorLocation();
	Loc.Z -= Pawn->GetSimpleCollisionHalfHeight();
	// Web rules (metres -> cm): not within 4.5 m of the town centre, 1.2 m of a fire, 1.5 m of a station.
	bool bBlocked = FVector2D(Loc.X, Loc.Y).Size() < 450.f;
	for (TActorIterator<AStation> It(World); It && !bBlocked; ++It)
	{
		const float Min = It->IsA<AGielinorFire>() ? 120.f : 150.f;
		if (FVector::Dist2D(It->GetActorLocation(), Loc) < Min) bBlocked = true;
	}
	if (bBlocked)
	{
		if (Ev) Ev->Message(LOCTEXT("NotHere", "You can't light a fire here."));
		return nullptr;
	}
	if (!S->Inventory->RemoveFromSlot(InventorySlot, 1)) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* Cls = FireClass ? FireClass.Get() : AGielinorFire::StaticClass();
	AGielinorFire* Fire = World->SpawnActor<AGielinorFire>(Cls, Loc, FRotator::ZeroRotator, P);
	if (Ev) Ev->Message(LOCTEXT("Catches", "The fire catches and the logs begin to burn."));
	return Fire;
}

#undef LOCTEXT_NAMESPACE
