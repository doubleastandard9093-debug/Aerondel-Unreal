#include "Core/GielinorGameMode.h"
#include "World/GielinorScalability.h"
#include "World/GielinorEnvironment.h"
#include "World/GielinorHighDetail.h"
#include "Core/GielinorPlayerState.h"
#include "Core/PlatformModeSubsystem.h"
#include "Player/GielinorCharacter.h"
#include "Player/GielinorPlayerController.h"
#include "XR/GielinorVRPawn.h"
#include "UI/GielinorHUD.h"
#include "Engine/GameInstance.h"

AGielinorGameMode::AGielinorGameMode()
{
	PlayerStateClass = AGielinorPlayerState::StaticClass();
	PlayerControllerClass = AGielinorPlayerController::StaticClass();
	HUDClass = AGielinorHUD::StaticClass();
	DesktopPawnClass = AGielinorCharacter::StaticClass();
	VRPawnClass = AGielinorVRPawn::StaticClass();
	DefaultPawnClass = DesktopPawnClass;
}

UClass* AGielinorGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UPlatformModeSubsystem* Mode = GI->GetSubsystem<UPlatformModeSubsystem>())
		{
			return Mode->IsVR() ? VRPawnClass.Get() : DesktopPawnClass.Get();
		}
	}
	return DesktopPawnClass.Get();
}

void AGielinorGameMode::BeginPlay()
{
	Super::BeginPlay();
	UGielinorScalability::ApplySavedTier(this);
	// Tier-driven sky / fog / post / clouds; follows Settings changes live.
	if (UWorld* W = GetWorld()) { W->SpawnActor<AGielinorEnvironment>(); W->SpawnActor<AGielinorHighDetail>(); }
}
