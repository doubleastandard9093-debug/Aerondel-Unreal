#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GielinorGameMode.generated.h"

UCLASS()
class GIELINOR_API AGielinorGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AGielinorGameMode();
	/** Desktop vs VR pawn is chosen per-player by UPlatformModeSubsystem. */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	/** Applies the saved graphics tier (Low by default). */
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category="Gielinor") TSubclassOf<APawn> DesktopPawnClass;
	UPROPERTY(EditDefaultsOnly, Category="Gielinor") TSubclassOf<APawn> VRPawnClass;
};
