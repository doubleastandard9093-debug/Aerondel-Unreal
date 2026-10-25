#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GielinorEvents.generated.h"

class UItemDef; class USkillDef; class UMobDef; class UNpcDef; class UQuestDef;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnXpGained, USkillDef*, Skill, float, Amount, float, TotalXp, int32, Level);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLevelUp, USkillDef*, Skill, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemGained, UItemDef*, Item, int32, Count);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMobKilled, UMobDef*, Mob);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNpcTalked, UNpcDef*, Npc);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestChanged, UQuestDef*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBonesBuried, UItemDef*, Bones);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlaceVisited, FName, PlaceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameMessage, const FText&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogue, const FText&, Speaker, const TArray<FText>&, Lines);

/**
 * Game-wide event bus (GameplayMessage-style, without the plugin dependency).
 * Survives pawn swaps and level loads; UI and quests subscribe here instead of to specific actors.
 */
UCLASS()
class GIELINOR_API UGielinorEvents : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnXpGained OnXpGained;
	UPROPERTY(BlueprintAssignable) FOnLevelUp OnLevelUp;
	UPROPERTY(BlueprintAssignable) FOnItemGained OnItemGained;
	UPROPERTY(BlueprintAssignable) FOnMobKilled OnMobKilled;
	UPROPERTY(BlueprintAssignable) FOnBonesBuried OnBonesBuried;
	UPROPERTY(BlueprintAssignable) FOnPlaceVisited OnPlaceVisited;
	UPROPERTY(BlueprintAssignable) FOnNpcTalked OnNpcTalked;
	UPROPERTY(BlueprintAssignable) FOnQuestChanged OnQuestChanged;
	UPROPERTY(BlueprintAssignable) FOnGameMessage OnMessage;
	UPROPERTY(BlueprintAssignable) FOnDialogue OnDialogue;

	UFUNCTION(BlueprintCallable, Category="Gielinor") void Message(const FText& Text) { OnMessage.Broadcast(Text); }

	static UGielinorEvents* Get(const UObject* WorldContext);
};
