#include "Core/PlatformModeSubsystem.h"
#include "Core/AerondelGameMode.h"
#include "Aerondel.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UPlatformModeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FParse::Param(FCommandLine::Get(), TEXT("vr"))) { bForced = true; Mode = EAerondelPlayMode::VR; }
	else if (FParse::Param(FCommandLine::Get(), TEXT("novr"))) { bForced = true; Mode = EAerondelPlayMode::Desktop; }
	else { Mode = DetectHMD() ? EAerondelPlayMode::VR : EAerondelPlayMode::Desktop; }
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UPlatformModeSubsystem::Tick), 0.5f);
}

void UPlatformModeSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

bool UPlatformModeSubsystem::DetectHMD()
{
	return UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled()
		&& UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayConnected();
}

bool UPlatformModeSubsystem::Tick(float)
{
	if (!bForced)
	{
		const EAerondelPlayMode Detected = DetectHMD() ? EAerondelPlayMode::VR : EAerondelPlayMode::Desktop;
		if (Detected != Mode) ApplyMode(Detected);
	}
	return true;
}

void UPlatformModeSubsystem::ForceMode(EAerondelPlayMode NewMode)
{
	bForced = true;
	if (NewMode != Mode) ApplyMode(NewMode);
}

void UPlatformModeSubsystem::ApplyMode(EAerondelPlayMode NewMode)
{
	Mode = NewMode;
	UE_LOG(LogAerondel, Log, TEXT("Play mode -> %s"), IsVR() ? TEXT("VR") : TEXT("Desktop"));
	if (IsVR()) UHeadMountedDisplayFunctionLibrary::EnableHMD(true);
	SwapPawn();
	OnModeChanged.Broadcast(Mode);
}

void UPlatformModeSubsystem::SwapPawn()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return;
	AAerondelGameMode* GM = World->GetAuthGameMode<AAerondelGameMode>();
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!GM || !PC) return;

	APawn* Old = PC->GetPawn();
	TSubclassOf<APawn> Wanted = IsVR() ? GM->VRPawnClass : GM->DesktopPawnClass;
	if (!Wanted || (Old && Old->IsA(Wanted))) return;

	FTransform Xf = Old ? FTransform(FRotator(0, Old->GetActorRotation().Yaw, 0), Old->GetActorLocation()) : FTransform::Identity;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* NewPawn = World->SpawnActor<APawn>(Wanted, Xf, P);
	if (!NewPawn) return;
	PC->UnPossess();
	if (Old) Old->Destroy();
	PC->Possess(NewPawn); // PlayerState (skills, inventory, quests...) carries over untouched.
}
