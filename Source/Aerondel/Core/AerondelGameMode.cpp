#include "Core/AerondelGameMode.h"
#include "World/AerondelScalability.h"
#include "World/AerondelEnvironment.h"
#include "World/AerondelHighDetail.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlatformModeSubsystem.h"
#include "Player/AerondelCharacter.h"
#include "Player/AerondelPlayerController.h"
#include "XR/AerondelVRPawn.h"
#include "UI/AerondelHUD.h"
#include "Engine/GameInstance.h"

AAerondelGameMode::AAerondelGameMode()
{
	PlayerStateClass = AAerondelPlayerState::StaticClass();
	PlayerControllerClass = AAerondelPlayerController::StaticClass();
	HUDClass = AAerondelHUD::StaticClass();
	DesktopPawnClass = AAerondelCharacter::StaticClass();
	VRPawnClass = AAerondelVRPawn::StaticClass();
	DefaultPawnClass = DesktopPawnClass;
}

UClass* AAerondelGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
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

void AAerondelGameMode::BeginPlay()
{
	Super::BeginPlay();
	UAerondelScalability::ApplySavedTier(this);
	// Tier-driven sky / fog / post / clouds; follows Settings changes live.
	if (UWorld* W = GetWorld()) { W->SpawnActor<AAerondelEnvironment>(); W->SpawnActor<AAerondelHighDetail>(); }
}
