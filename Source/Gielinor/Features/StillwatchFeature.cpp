#include "Features/StillwatchFeature.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

bool UStillwatchFeature::Trigger(bool bVsBoss)
{
	UGameInstance* GI = GetGameInstance();
	UWorld* W = GI ? GI->GetWorld() : nullptr;
	if (!W || bActive || Meter < Max) return false;
	Meter = 0.f; ++Uses; bActive = true;
	UGameplayStatics::SetGlobalTimeDilation(W, Scale);
	if (APawn* P = UGameplayStatics::GetPlayerPawn(W, 0)) P->CustomTimeDilation = 1.f / Scale;
	// Timer runs in dilated time, so scale the real duration.
	W->GetTimerManager().SetTimer(EndTimer, FTimerDelegate::CreateUObject(this, &UStillwatchFeature::End), (bVsBoss ? BossSeconds : Seconds) * Scale, false);
	Say(TEXT("Stillwatch: the world slows. Aim for a weak spot."));
	return true;
}

void UStillwatchFeature::End()
{
	bActive = false;
	UGameInstance* GI = GetGameInstance();
	if (UWorld* W = GI ? GI->GetWorld() : nullptr)
	{
		UGameplayStatics::SetGlobalTimeDilation(W, 1.f);
		if (APawn* P = UGameplayStatics::GetPlayerPawn(W, 0)) P->CustomTimeDilation = 1.f;
	}
}

void UStillwatchFeature::SaveState(FJsonObject& Out) const
{
	Out.SetNumberField(TEXT("meter"), Meter); Out.SetNumberField(TEXT("uses"), Uses);
	Out.SetStringField(TEXT("spot"), Spot == EWeakSpot::Legs ? TEXT("legs") : Spot == EWeakSpot::Arms ? TEXT("arms") : TEXT("head"));
}

void UStillwatchFeature::LoadState(const FJsonObject& In)
{
	Meter = FMath::Clamp((float)JNum(In, TEXT("meter")), 0.f, Max); Uses = FMath::Max(0, (int32)JInt(In, TEXT("uses")));
	const FString S = JStr(In, TEXT("spot"));
	Spot = S == TEXT("legs") ? EWeakSpot::Legs : S == TEXT("arms") ? EWeakSpot::Arms : EWeakSpot::Head;
}
