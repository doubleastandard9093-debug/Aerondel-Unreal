#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AerondelGameMode.generated.h"

UCLASS()
class AERONDEL_API AAerondelGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AAerondelGameMode();
	/** Desktop vs VR pawn is chosen per-player by UPlatformModeSubsystem. */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	/** Applies the saved graphics tier (Low by default). */
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category="Aerondel") TSubclassOf<APawn> DesktopPawnClass;
	UPROPERTY(EditDefaultsOnly, Category="Aerondel") TSubclassOf<APawn> VRPawnClass;
};
