#include "World/AerondelHighDetail.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"

static const TCHAR* GHighMaster = TEXT("/Game/Gielinor/Materials/High/M_TintHigh.M_TintHigh");

AAerondelHighDetail::AAerondelHighDetail()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AAerondelHighDetail::BeginPlay()
{
	Super::BeginPlay();
	TierHandle = UAerondelScalability::OnTierApplied.AddUObject(this, &AAerondelHighDetail::Apply);
	Apply(UAerondelScalability::GetAppliedTier());
}

void AAerondelHighDetail::EndPlay(const EEndPlayReason::Type Reason)
{
	UAerondelScalability::OnTierApplied.Remove(TierHandle);
	Super::EndPlay(Reason);
}

void AAerondelHighDetail::Apply(EAerondelTier Tier)
{
	const bool bNowRich = Tier >= EAerondelTier::High;
	const bool bNowUltra = Tier == EAerondelTier::Ultra;
	if (!bNowRich)
	{
		RestoreMaterials(); ClearFoliage();
		HighMaster = nullptr; // let GC drop the High material; Low keeps nothing High in memory
		bRich = bUltra = false;
		return;
	}
	if (!HighMaster) HighMaster = LoadObject<UMaterialInterface>(nullptr, GHighMaster);
	if (!HighMaster) { UE_LOG(LogTemp, Warning, TEXT("Aerondel: %s missing; run Content/Python/build_high_fidelity.py"), GHighMaster); }
	const bool bDensityChanged = bUltra != bNowUltra;
	bRich = true; bUltra = bNowUltra;
	UpgradeMaterials();
	if (bDensityChanged || !Foliage.Num()) { ClearFoliage(); ScatterCentre = FVector2D(1e9, 1e9); }
}

void AAerondelHighDetail::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bRich) return;
	// Mobs, NPCs and ships spawn over time: upgrade newcomers every few seconds. Foliage follows the player.
	RescanTimer += Dt;
	if (RescanTimer >= 4.f) { RescanTimer = 0.f; UpgradeMaterials(); }
	if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector2D At(P->GetActorLocation().X, P->GetActorLocation().Y);
		if (FVector2D::Distance(At, ScatterCentre) > 4000.f) { ScatterCentre = At; ClearFoliage(); Scatter(bUltra); }
	}
}

static bool IsTintInstance(UMaterialInterface* M)
{
	for (UMaterialInstance* MI = Cast<UMaterialInstance>(M); MI; MI = Cast<UMaterialInstance>(MI->Parent))
	{
		if (MI->Parent && MI->Parent->GetFName() == TEXT("M_Tint")) return true;
		if (MI->Parent && MI->Parent->GetFName() == TEXT("M_TintHigh")) return false; // already upgraded
	}
	return false;
}

void AAerondelHighDetail::UpgradeMaterials()
{
	if (!HighMaster) return;
	for (TObjectIterator<UMeshComponent> It; It; ++It)
	{
		UMeshComponent* C = *It;
		if (!C || C->GetWorld() != GetWorld() || C->GetOwner() == this || !C->IsRegistered()) continue;
		for (int32 Slot = 0; Slot < C->GetNumMaterials(); ++Slot)
		{
			UMaterialInterface* M = C->GetMaterial(Slot);
			if (!IsTintInstance(M)) continue;
			FLinearColor Col(0.8f, 0.8f, 0.8f); float Rough = 0.8f, Metal = 0.f;
			M->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), Col);
			M->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Roughness")), Rough);
			M->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Metallic")), Metal);
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(HighMaster, C);
			D->SetVectorParameterValue(TEXT("Color"), Col);
			D->SetScalarParameterValue(TEXT("Roughness"), Rough);
			D->SetScalarParameterValue(TEXT("Metallic"), Metal);
			D->SetScalarParameterValue(TEXT("Water"), M->GetName().Contains(TEXT("Water")) ? 1.f : 0.f);
			Swaps.Add({ C, Slot, M });
			C->SetMaterial(Slot, D);
		}
	}
}

void AAerondelHighDetail::RestoreMaterials()
{
	for (const FSwap& S : Swaps)
		if (S.Comp.IsValid() && S.Original.IsValid()) S.Comp->SetMaterial(S.Slot, S.Original.Get());
	Swaps.Reset();
}

UInstancedStaticMeshComponent* AAerondelHighDetail::MakeIsm(const TCHAR* Mesh, FLinearColor Color, float Rough, bool bShadow)
{
	UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, Mesh);
	if (!SM) return nullptr;
	UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this);
	I->SetStaticMesh(SM);
	I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	I->SetCastShadow(bShadow);
	I->SetCullDistances(bUltra ? 4500 : 3000, bUltra ? 6000 : 4000);
	I->SetupAttachment(RootComponent);
	I->RegisterComponent();
	if (HighMaster)
	{
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(HighMaster, I);
		D->SetVectorParameterValue(TEXT("Color"), Color);
		D->SetScalarParameterValue(TEXT("Roughness"), Rough);
		I->SetMaterial(0, D);
	}
	Foliage.Add(I);
	return I;
}

void AAerondelHighDetail::Scatter(bool bDense)
{
	UWorld* W = GetWorld();
	if (!W) return;
	UInstancedStaticMeshComponent* GrassA = MakeIsm(TEXT("/Engine/BasicShapes/Cone.Cone"), FLinearColor(0.20f, 0.42f, 0.14f), 0.9f, false);
	UInstancedStaticMeshComponent* GrassB = MakeIsm(TEXT("/Engine/BasicShapes/Cone.Cone"), FLinearColor(0.32f, 0.52f, 0.18f), 0.9f, false);
	UInstancedStaticMeshComponent* FlowerY = MakeIsm(TEXT("/Engine/BasicShapes/Sphere.Sphere"), FLinearColor(0.95f, 0.82f, 0.2f), 0.6f, false);
	UInstancedStaticMeshComponent* FlowerV = MakeIsm(TEXT("/Engine/BasicShapes/Sphere.Sphere"), FLinearColor(0.6f, 0.35f, 0.85f), 0.6f, false);
	UInstancedStaticMeshComponent* Pebble = MakeIsm(TEXT("/Engine/BasicShapes/Sphere.Sphere"), FLinearColor(0.45f, 0.44f, 0.42f), 0.95f, true);
	if (!GrassA || !GrassB) return;

	const int32 Count = bDense ? 9000 : 4000;
	const float Radius = 5000.f; // 50 m around the player, rebuilt when they move 40 m
	FRandomStream R(FCrc::StrCrc32(*FString::Printf(TEXT("%d_%d"), FMath::RoundToInt(ScatterCentre.X / 4000.f), FMath::RoundToInt(ScatterCentre.Y / 4000.f))));
	FCollisionQueryParams Q(TEXT("AerondelFoliage"), false, this);
	if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0)) Q.AddIgnoredActor(P);
	TArray<FTransform> GA, GB, FY, FV, PB;
	for (int32 i = 0; i < Count; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI), D = Radius * FMath::Sqrt(R.FRand());
		const FVector XY(ScatterCentre.X + FMath::Cos(A) * D, ScatterCentre.Y + FMath::Sin(A) * D, 0.f);
		FHitResult Hit;
		if (!W->LineTraceSingleByChannel(Hit, XY + FVector(0, 0, 3000.f), XY - FVector(0, 0, 2000.f), ECC_Visibility, Q)) continue;
		const UPrimitiveComponent* HC = Hit.GetComponent();
		const UMaterialInterface* HM = HC ? HC->GetMaterial(0) : nullptr;
		if (!HM || !HM->GetName().Contains(TEXT("Grass")) || Hit.ImpactNormal.Z < 0.85f) continue; // grass only, no slopes/roofs
		const float Roll = R.FRand();
		const FRotator Rot(0.f, R.FRandRange(0.f, 360.f), 0.f);
		if (Roll < 0.86f)
		{
			const float H = R.FRandRange(0.12f, 0.3f);
			(Roll < 0.43f ? GA : GB).Add(FTransform(Rot, Hit.ImpactPoint + FVector(0, 0, H * 50.f), FVector(R.FRandRange(0.04f, 0.08f), R.FRandRange(0.04f, 0.08f), H)));
		}
		else if (Roll < 0.95f)
			(Roll < 0.905f ? FY : FV).Add(FTransform(Rot, Hit.ImpactPoint + FVector(0, 0, 14.f), FVector(0.07f)));
		else
			PB.Add(FTransform(Rot, Hit.ImpactPoint + FVector(0, 0, 3.f), FVector(R.FRandRange(0.08f, 0.22f), R.FRandRange(0.08f, 0.2f), R.FRandRange(0.05f, 0.1f))));
	}
	GrassA->AddInstances(GA, false, true); GrassB->AddInstances(GB, false, true);
	if (FlowerY) FlowerY->AddInstances(FY, false, true);
	if (FlowerV) FlowerV->AddInstances(FV, false, true);
	if (Pebble) Pebble->AddInstances(PB, false, true);
}

void AAerondelHighDetail::ClearFoliage()
{
	for (UInstancedStaticMeshComponent* I : Foliage) if (I) I->DestroyComponent();
	Foliage.Reset();
}
