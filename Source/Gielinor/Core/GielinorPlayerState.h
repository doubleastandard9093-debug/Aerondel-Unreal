#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GielinorPlayerState.generated.h"

class USkillsComponent; class UInventoryComponent; class UEquipmentComponent; class UBankComponent; class UQuestComponent; class UHealthComponent;

/**
 * All persistent player progression lives here, not on the pawn, so the desktop Character and the
 * VR pawn can be swapped by UPlatformModeSubsystem without losing state.
 * (PlayerState is also the natural replication home once multiplayer resumes.)
 */
UCLASS()
class GIELINOR_API AGielinorPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	AGielinorPlayerState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gielinor") TObjectPtr<USkillsComponent> Skills;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gielinor") TObjectPtr<UInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gielinor") TObjectPtr<UEquipmentComponent> Equipment;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gielinor") TObjectPtr<UBankComponent> Bank;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gielinor") TObjectPtr<UQuestComponent> Quests;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gielinor") TObjectPtr<UHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gielinor") TObjectPtr<class UPlayerStatsComponent> Stats;

	virtual void BeginPlay() override;

	/** Resolve the player state from any pawn/controller/actor owned by the local player. */
	UFUNCTION(BlueprintPure, Category="Gielinor", meta=(DefaultToSelf="Actor")) static AGielinorPlayerState* FromActor(const AActor* Actor);
};
