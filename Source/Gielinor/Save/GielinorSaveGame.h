#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GielinorSaveGame.generated.h"

USTRUCT() struct FSavedSkill { GENERATED_BODY() UPROPERTY() FName Id; UPROPERTY() float Xp = 0.f; };
USTRUCT() struct FSavedSlot { GENERATED_BODY() UPROPERTY() int32 Index = 0; UPROPERTY() FName ItemId; UPROPERTY() int32 Count = 0; };
USTRUCT() struct FSavedEquip { GENERATED_BODY() UPROPERTY() uint8 Slot = 0; UPROPERTY() FName ItemId; };
USTRUCT() struct FSavedQuest { GENERATED_BODY() UPROPERTY() FName Id; UPROPERTY() uint8 State = 0; UPROPERTY() int32 Step = 0; UPROPERTY() int32 Counter = 0; };

/** Content is referenced by stable string ids (not asset paths) so renames/moves don't break saves. */
UCLASS()
class GIELINOR_API UGielinorSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 Version = 1;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() int32 Hitpoints = 10;
	UPROPERTY() TArray<FSavedSkill> Skills;
	UPROPERTY() TArray<FSavedSlot> Inventory;
	UPROPERTY() TArray<FSavedSlot> Bank;
	UPROPERTY() TArray<FSavedEquip> Equipment;
	UPROPERTY() TArray<FSavedQuest> Quests;
};
