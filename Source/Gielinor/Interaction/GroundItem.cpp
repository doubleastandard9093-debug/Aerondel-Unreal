#include "Interaction/GroundItem.h"
#include "Data/ItemDef.h"
#include "Core/GielinorPlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AGroundItem::AGroundItem()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetRelativeScale3D(FVector(0.25f));
	Mesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // clickable
	Range = 150.f;
}

void AGroundItem::BeginPlay()
{
	Super::BeginPlay();
	if (Item && !Item->WorldMesh.IsNull()) if (UStaticMesh* M = Item->WorldMesh.LoadSynchronous()) { Mesh->SetStaticMesh(M); Mesh->SetRelativeScale3D(FVector(1)); }
	SetLifeSpan(DespawnSeconds);
}

FText AGroundItem::GetInteractLabel() const
{
	return FText::Format(NSLOCTEXT("Gielinor", "Take", "Take {0}"), Item ? Item->DisplayName : FText::GetEmpty());
}

void AGroundItem::Interact(AActor* InInstigator)
{
	AGielinorPlayerState* S = PS(InInstigator);
	if (!S || !Item) return;
	const int32 Left = S->Inventory->Add(Item, Count);
	if (Left == 0) Destroy(); else Count = Left;
}

AGroundItem* AGroundItem::Spawn(UWorld* InWorld, UItemDef* InItem, int32 InCount, const FVector& Where)
{
	if (!InWorld || !InItem) return nullptr;
	AGroundItem* G = InWorld->SpawnActorDeferred<AGroundItem>(AGroundItem::StaticClass(), FTransform(Where), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!G) return nullptr;
	G->Item = InItem; G->Count = InCount; G->DisplayName = InItem->DisplayName;
	G->FinishSpawning(FTransform(Where));
	return G;
}
