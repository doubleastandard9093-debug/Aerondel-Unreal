#include "Features/SailingVisuals.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Features/SailingFeature.h"
#include "World/GielinorScalability.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UStaticMesh* Shape(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}
}

ASailingVisuals::ASailingVisuals()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;
	Deck = CreateDefaultSubobject<UBoxComponent>(TEXT("Deck"));
	Deck->SetupAttachment(SceneRoot);
	Deck->SetBoxExtent(FVector(400.f, 160.f, 15.f));
	Deck->SetCollisionProfileName(TEXT("BlockAll"));
	Deck->SetAbsolute(true, true, true);
	Balls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Balls"));
	Balls->SetupAttachment(SceneRoot);
	Balls->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Balls->SetAbsolute(true, true, true);
	ChartMarks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ChartMarks"));
	ChartMarks->SetupAttachment(SceneRoot);
	ChartMarks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChartMarks->SetAbsolute(true, true, true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Sphere.Succeeded()) Balls->SetStaticMesh(Sphere.Object);
	if (Cyl.Succeeded()) ChartMarks->SetStaticMesh(Cyl.Object);
	if (Mat.Succeeded()) BaseMat = Mat.Object;
}

void ASailingVisuals::BeginPlay()
{
	Super::BeginPlay();
	bLow = UGielinorScalability::GetSavedTier() <= EGielinorTier::Low;
	auto Tint = [this](UInstancedStaticMeshComponent* C, const FLinearColor& Col)
	{
		if (!BaseMat) return;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BaseMat, this);
		M->SetVectorParameterValue(TEXT("Color"), Col);
		C->SetMaterial(0, M);
	};
	Tint(Balls, FLinearColor(0.05f, 0.05f, 0.06f));
	Tint(ChartMarks, FLinearColor(0.8f, 0.1f, 0.1f));
	Mine.Root = NewObject<USceneComponent>(this, TEXT("MyShip"));
	Mine.Root->SetupAttachment(SceneRoot); Mine.Root->SetAbsolute(true, true, true); Mine.Root->RegisterComponent();
}

UStaticMeshComponent* ASailingVisuals::Part(FShipParts& S, const TCHAR* ShapeName, const FVector& Loc, const FVector& Scale, const FLinearColor& Color, const FRotator& Rot)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Shape(ShapeName));
	C->SetupAttachment(S.Root);
	C->SetRelativeLocationAndRotation(Loc, Rot);
	C->SetRelativeScale3D(Scale);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(!bLow);
	if (BaseMat)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BaseMat, this);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, M);
	}
	C->RegisterComponent();
	S.Parts.Add(C);
	return C;
}

void ASailingVisuals::BuildShip(FShipParts& S, int32 Kind, bool bEnemy)
{
	for (UStaticMeshComponent* C : S.Parts) if (C) C->DestroyComponent();
	S.Parts.Reset();
	S.BuiltKind = Kind;
	// Size by kind: yours 1 sloop / 2 brigantine / 3 galleon; enemies 0 sloop / 1 brig. Basic shapes are 100 cm.
	const int32 Size = bEnemy ? Kind + 1 : FMath::Clamp(Kind, 1, 3);
	const float L = 6.f + Size * 2.f, W = 2.2f + Size * 0.5f; // metres
	const FLinearColor Wood = bEnemy ? FLinearColor(0.12f, 0.16f, 0.14f) : FLinearColor(0.42f, 0.27f, 0.14f);
	const FLinearColor Trim = bEnemy ? FLinearColor(0.2f, 0.35f, 0.3f) : FLinearColor(0.7f, 0.55f, 0.25f);
	const FLinearColor Cloth = bEnemy ? FLinearColor(0.18f, 0.2f, 0.18f) : FLinearColor(0.9f, 0.87f, 0.78f);
	// Forward is +X in the ship root.
	Part(S, TEXT("Cube"), FVector(0, 0, 40), FVector(L, W, 1.2f), Wood);                                   // hull
	Part(S, TEXT("Cone"), FVector(L * 50.f + 60.f, 0, 40), FVector(W * 0.95f, 1.2f, 2.f), Wood, FRotator(-90, 0, 0)); // bow
	Part(S, TEXT("Cube"), FVector(0, 0, 105), FVector(L * 0.98f, W * 0.92f, 0.12f), Trim);                // deck rail band
	Part(S, TEXT("Cube"), FVector(-L * 38.f, 0, 150), FVector(L * 0.22f, W * 0.9f, 0.9f), Wood);          // stern castle
	const int32 Masts = Size >= 3 ? 3 : Size >= 2 ? 2 : 1;
	for (int32 i = 0; i < Masts; ++i)
	{
		const float Mx = Masts == 1 ? 0.f : (i - (Masts - 1) * 0.5f) * L * 30.f;
		const float H = 7.f + Size;
		Part(S, TEXT("Cylinder"), FVector(Mx, 0, 100 + H * 50.f), FVector(0.25f, 0.25f, H), Trim * 0.6f);
		if (!bLow) Part(S, TEXT("Cube"), FVector(Mx + 10.f, 0, 120 + H * 55.f), FVector(0.05f, W * 1.3f, H * 0.6f), Cloth); // sail cloth: High+ only
	}
	const int32 Guns = Size + 1;
	for (int32 g = 0; g < Guns; ++g)
		for (int32 Side = -1; Side <= 1; Side += 2)
			Part(S, TEXT("Cylinder"), FVector((g - (Guns - 1) * 0.5f) * 140.f, Side * (W * 50.f + 20.f), 90), FVector(0.22f, 0.22f, 0.7f), FLinearColor(0.08f, 0.08f, 0.09f), FRotator(0, 0, 90));
}

void ASailingVisuals::PlaceShip(FShipParts& S, float X, float Z, float Yaw, bool bVisible)
{
	if (!S.Root) return;
	S.Root->SetVisibility(bVisible, true);
	// Web yaw 0 faces +z (UE +Y): UE yaw = 90 - deg(yaw).
	S.Root->SetWorldLocationAndRotation(FVector(X * 100.f, Z * 100.f, 0.f), FRotator(0.f, 90.f - FMath::RadiansToDegrees(Yaw), 0.f));
}

void ASailingVisuals::Tick(float Dt)
{
	Super::Tick(Dt);
	UGameInstance* GI = GetGameInstance();
	USailingFeature* Sea = GI ? GI->GetSubsystem<USailingFeature>() : nullptr;
	if (!Sea) return;
	const bool bShow = Sea->IsAtSea() || !Sea->GetMoored().IsNone();
	const FSeaBoat Ship = Sea->GetShip();
	// Helm from the pawn's move input (WASD / left stick): sideways = turn, forward = full sail, back = strike sail.
	if (Sea->IsAtSea())
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			const FVector In = Pawn->GetLastMovementInputVector().GetSafeNormal2D();
			const float UeYaw = FMath::DegreesToRadians(90.f - FMath::RadiansToDegrees(Ship.Yaw));
			const FVector Fwd(FMath::Cos(UeYaw), FMath::Sin(UeYaw), 0.f), Right(-Fwd.Y, Fwd.X, 0.f);
			const float F = FVector::DotProduct(In, Fwd), R = FVector::DotProduct(In, Right);
			Sea->Helm(FMath::Abs(R) > 0.3f ? R : 0.f, F > 0.5f ? 1.f : F < -0.5f ? 0.f : -1.f);
		}
	if (Sea->GetHull() > 0 && Mine.BuiltKind != Sea->GetHull()) BuildShip(Mine, Sea->GetHull(), false);
	PlaceShip(Mine, Ship.X, Ship.Z, Ship.Yaw, bShow && Sea->GetHull() > 0);
	// The deck follows your ship while you are at sea; parked far below otherwise.
	Deck->SetWorldLocationAndRotation(Sea->IsAtSea() ? FVector(Ship.X * 100.f, Ship.Z * 100.f, DeckTopCm - 15.f) : FVector(0, 0, -100000.f),
		FRotator(0.f, 90.f - FMath::RadiansToDegrees(Ship.Yaw), 0.f));

	const TArray<FSeaBoat> En = Sea->GetEnemies();
	while (Foes.Num() < En.Num())
	{
		FShipParts& F = Foes.AddDefaulted_GetRef();
		F.Root = NewObject<USceneComponent>(this);
		F.Root->SetupAttachment(SceneRoot); F.Root->SetAbsolute(true, true, true); F.Root->RegisterComponent();
	}
	for (int32 i = 0; i < Foes.Num(); ++i)
	{
		if (i < En.Num())
		{
			if (Foes[i].BuiltKind != En[i].Kind) BuildShip(Foes[i], En[i].Kind, true);
			PlaceShip(Foes[i], En[i].X, En[i].Z, En[i].Yaw, Sea->IsAtSea());
		}
		else PlaceShip(Foes[i], 0, 0, 0, false);
	}

	Balls->ClearInstances();
	if (Sea->IsAtSea())
		for (const FCannonball& B : Sea->GetBalls())
			Balls->AddInstance(FTransform(FRotator::ZeroRotator, FVector(B.Pos.X * 100.f, B.Pos.Z * 100.f, B.Pos.Y * 100.f), FVector(0.35f)), true);
	ChartMarks->ClearInstances();
	for (const FTreasureChart& C : Sea->GetCharts())
		ChartMarks->AddInstance(FTransform(FRotator::ZeroRotator, FVector(C.X * 100.f, C.Z * 100.f, 20.f), FVector(1.2f, 1.2f, 0.05f)), true);
}
