#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AerondelPlayerState.generated.h"

class USkillsComponent; class UInventoryComponent; class UEquipmentComponent; class UBankComponent; class UQuestComponent; class UHealthComponent;

/**
 * All persistent player progression lives here, not on the pawn, so the desktop Character and the
 * VR pawn can be swapped by UPlatformModeSubsystem without losing state.
 * (PlayerState is also the natural replication home once multiplayer resumes.)
 */
UCLASS()
class AERONDEL_API AAerondelPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	AAerondelPlayerState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aerondel") TObjectPtr<USkillsComponent> Skills;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aerondel") TObjectPtr<UInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aerondel") TObjectPtr<UEquipmentComponent> Equipment;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aerondel") TObjectPtr<UBankComponent> Bank;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aerondel") TObjectPtr<UQuestComponent> Quests;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aerondel") TObjectPtr<UHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aerondel") TObjectPtr<class UPlayerStatsComponent> Stats;

	virtual void BeginPlay() override;

	/** Resolve the player state from any pawn/controller/actor owned by the local player. */
	UFUNCTION(BlueprintPure, Category="Aerondel", meta=(DefaultToSelf="Actor")) static AAerondelPlayerState* FromActor(const AActor* Actor);
};
