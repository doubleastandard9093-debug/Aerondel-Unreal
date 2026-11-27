#include "Combat/AerondelFloatText.h"
#include "World/AerondelScalability.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AAerondelFloatText::AAerondelFloatText()
{
	PrimaryActorTick.bCanEverTick = true;
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	RootComponent = Label;
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextBottom);
	Label->SetWorldSize(28.f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetCastShadow(false);
	Label->SetHiddenInGame(false);
}

void AAerondelFloatText::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	AddActorWorldOffset(FVector(0, 0, Rise * Dt));
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		FVector To = Cam->GetCameraLocation() - GetActorLocation(); To.Z = 0.f;
		if (!To.IsNearlyZero()) SetActorRotation(To.Rotation());
	}
	if (Label) Label->SetWorldSize(FMath::Lerp(28.f, 18.f, FMath::Clamp(Age / 0.85f, 0.f, 1.f)));
	if (Age > 0.85f) Destroy();
}

void AAerondelFloatText::Show(const UObject* WorldContext, const FVector& WorldLocation, int32 Amount, bool bHeal)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return;
	const EAerondelTier Tier = UAerondelScalability::GetSavedTier();
	const int32 Cap = (Tier == EAerondelTier::Low || Tier == EAerondelTier::Quest) ? 6 : 16;
	int32 Live = 0;
	AAerondelFloatText* Oldest = nullptr;
	float OldestAge = -1.f;
	for (TActorIterator<AAerondelFloatText> It(W); It; ++It)
	{
		++Live;
		if (It->Age > OldestAge) { OldestAge = It->Age; Oldest = *It; }
	}
	if (Live >= Cap)
	{
		if (Oldest) Oldest->Destroy();
		else return; // Low: drop rather than spawn more
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Jitter(FMath::FRandRange(-18.f, 18.f), FMath::FRandRange(-18.f, 18.f), 0.f);
	AAerondelFloatText* A = W->SpawnActor<AAerondelFloatText>(WorldLocation + Jitter, FRotator::ZeroRotator, P);
	if (!A || !A->Label) return;
	if (bHeal)
	{
		A->Label->SetText(FText::FromString(FString::Printf(TEXT("+%d"), Amount)));
		A->Label->SetTextRenderColor(FColor(80, 220, 90));
	}
	else if (Amount <= 0)
	{
		A->Label->SetText(NSLOCTEXT("Aerondel", "Miss", "0"));
		A->Label->SetTextRenderColor(FColor(180, 180, 180));
	}
	else
	{
		A->Label->SetText(FText::FromString(FString::Printf(TEXT("%d"), Amount)));
		A->Label->SetTextRenderColor(FColor(255, 220, 80));
	}
}

void AAerondelFloatText::ShowOn(const AActor* Target, int32 Amount, bool bHeal)
{
	if (!Target) return;
	FVector Loc = Target->GetActorLocation();
	if (const UCapsuleComponent* Cap = Target->FindComponentByClass<UCapsuleComponent>())
		Loc.Z += Cap->GetScaledCapsuleHalfHeight() + 20.f;
	else
		Loc.Z += 80.f;
	Show(Target, Loc, Amount, bHeal);
}
