#include "XR/VRHandComponent.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "XR/VRInteractionBridge.h"
#include "Interaction/GielinorInteractable.h"
#include "Core/GielinorPlayerState.h"
#include "Core/GielinorEvents.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Data/ItemDef.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "HeadMountedDisplayTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UVRHandComponent::UVRHandComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	InitSphereRadius(8.f);
	SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetGenerateOverlapEvents(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) PlaceholderToolMesh = Cube.Object;
}

void UVRHandComponent::BeginPlay()
{
	Super::BeginPlay();
	LastPos = GetComponentLocation();
	// Created at runtime (components made inside another component's constructor are not auto-registered).
	ToolMesh = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("ToolMesh"));
	ToolMesh->SetupAttachment(this);
	ToolMesh->RegisterComponent();
	ToolMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ToolMesh->SetStaticMesh(PlaceholderToolMesh);
	// Placeholder haft: 6 x 6 x 70 cm pointing forward from the grip.
	ToolMesh->SetRelativeLocation(FVector(30, 0, 0));
	ToolMesh->SetRelativeScale3D(FVector(0.7f, 0.06f, 0.06f));
	ToolMesh->SetVisibility(false);
	OnComponentBeginOverlap.AddDynamic(this, &UVRHandComponent::OnHandOverlap);
}

AActor* UVRHandComponent::BestOverlap() const
{
	TArray<AActor*> Actors; GetOverlappingActors(Actors);
	AActor* Best = nullptr; float BestD = TNumericLimits<float>::Max();
	for (AActor* A : Actors)
	{
		if (!Cast<IGielinorInteractable>(A) || A == GetOwner()) continue;
		const float D = FVector::DistSquared(A->GetActorLocation(), GetComponentLocation());
		if (D < BestD) { BestD = D; Best = A; }
	}
	return Best;
}

void UVRHandComponent::GripPressed()
{
	if (AActor* T = BestOverlap()) { UVRInteractionBridge::GrabOrPoke(GetOwner(), T); return; }
	DrawTool();
}

void UVRHandComponent::GripReleased()
{
	bHolding = false;
	if (ToolMesh) ToolMesh->SetVisibility(false);
	SetSphereRadius(8.f);
}

void UVRHandComponent::TriggerPressed()
{
	// Trigger = "use"/poke on whatever the hand is touching (UI is handled by WidgetInteraction).
	if (AActor* T = BestOverlap()) UVRInteractionBridge::GrabOrPoke(GetOwner(), T);
}

void UVRHandComponent::DrawTool()
{
	// Pick the tool for the nearest interactable within 3 m, else the weapon.
	TArray<FOverlapResult> Hits;
	GetWorld()->OverlapMultiByObjectType(Hits, GetComponentLocation(), FQuat::Identity, FCollisionObjectQueryParams::AllObjects, FCollisionShape::MakeSphere(300.f));
	AActor* Near = nullptr; float BestD = TNumericLimits<float>::Max();
	for (const FOverlapResult& H : Hits)
		if (AActor* A = H.GetActor())
			if (Cast<IGielinorInteractable>(A))
			{
				const float D = FVector::DistSquared(A->GetActorLocation(), GetComponentLocation());
				if (D < BestD) { BestD = D; Near = A; }
			}
	HeldTool = UVRInteractionBridge::ContextTool(Near);

	AGielinorPlayerState* PS = AGielinorPlayerState::FromActor(GetOwner());
	UItemDef* Item = nullptr;
	if (PS)
	{
		if (HeldTool != EToolType::None)
			Item = PS->Equipment->EquippedTool(HeldTool) ? PS->Equipment->EquippedTool(HeldTool) : PS->Inventory->BestTool(HeldTool);
		else
			Item = PS->Equipment->GetEquipped(EEquipSlot::Weapon);
	}
	if (HeldTool != EToolType::None && !Item)
	{
		if (UGielinorEvents* Ev = UGielinorEvents::Get(this)) Ev->Message(NSLOCTEXT("Gielinor", "NoToolVR", "You don't have a tool for that."));
		return;
	}
	bHolding = true;
	if (!ToolMesh) return;
	ToolMesh->SetVisibility(true);
	if (Item && !Item->WorldMesh.IsNull()) if (UStaticMesh* M = Item->WorldMesh.LoadSynchronous()) ToolMesh->SetStaticMesh(M);
	SetSphereRadius(20.f); // the tool head extends reach
}

void UVRHandComponent::OnHandOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!bHolding || Cooldown > 0.f || !Cast<IGielinorInteractable>(Other)) return;
	const float Speed = Velocity.Size();
	if (Speed < MinSwingSpeed) return;
	const float Power = FMath::GetMappedRangeValueClamped(FVector2D(MinSwingSpeed, FullPowerSpeed), FVector2D(0.3f, 1.f), Speed);
	if (UVRInteractionBridge::SwingHit(GetOwner(), Other, HeldTool, Power))
	{
		Cooldown = HitCooldown;
		// Short haptic buzz scaled by swing power (swap for a UHapticFeedbackEffect asset later).
		const APawn* Pawn = Cast<APawn>(GetOwner());
		if (APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
		{
			PC->SetHapticsByValue(0.5f, Power, Hand);
			TWeakObjectPtr<APlayerController> WeakPC(PC);
			const EControllerHand H = Hand;
			GetWorld()->GetTimerManager().SetTimer(HapticTimer, [WeakPC, H]() { if (WeakPC.IsValid()) WeakPC->SetHapticsByValue(0.f, 0.f, H); }, 0.12f, false);
		}
	}
}

void UVRHandComponent::UpdateHandTracking()
{
	// UE 5.5+: hand joints come from the XR system's hand-tracking state.
	if (!GEngine || !GEngine->XRSystem.IsValid()) return;
	FXRHandTrackingState Data;
	GEngine->XRSystem->GetHandTrackingState(this, EXRSpaceType::UnrealWorldSpace, Hand, Data);
	if (!Data.bValid) return;
	const int32 Thumb = (int32)EHandKeypoint::ThumbTip, Index = (int32)EHandKeypoint::IndexTip;
	if (!Data.HandKeyLocations.IsValidIndex(Thumb) || !Data.HandKeyLocations.IsValidIndex(Index)) return;
	const bool bPinch = (Data.HandKeyLocations[Thumb] - Data.HandKeyLocations[Index]).Size() < PinchCm;
	if (bPinch && !bPinching) GripPressed();
	if (!bPinch && bPinching) GripReleased();
	bPinching = bPinch;
}

void UVRHandComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* Fn)
{
	Super::TickComponent(Dt, TickType, Fn);
	const FVector P = GetComponentLocation();
	if (Dt > 0.f) Velocity = FMath::Lerp(Velocity, (P - LastPos) / Dt, 0.5f); // smoothed
	LastPos = P;
	Cooldown -= Dt;
	UpdateHandTracking();
}
