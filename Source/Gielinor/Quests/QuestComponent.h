#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/GielinorTypes.h"
#include "QuestComponent.generated.h"

class UQuestDef; class UNpcDef; class UItemDef; class UMobDef; class USkillDef;

USTRUCT(BlueprintType)
struct FQuestProgress
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UQuestDef> Quest = nullptr;
	UPROPERTY(BlueprintReadOnly) EQuestState State = EQuestState::NotStarted;
	UPROPERTY(BlueprintReadOnly) int32 Step = 0;
	/** Progress toward the current Gather/Kill step. */
	UPROPERTY(BlueprintReadOnly) int32 Counter = 0;
};

/**
 * Data-driven quest runner. Listens to the event bus (kills, item gains) and is told about NPC talks
 * by ANpcTalk. Steps: Talk (speak to Npc), Gather (obtain N of Item), Kill (kill N of Mob),
 * Deliver (talk to Npc while holding N of Item; items are consumed).
 */
UCLASS(ClassGroup=(Gielinor), meta=(BlueprintSpawnableComponent))
class GIELINOR_API UQuestComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="Quests") bool StartQuest(UQuestDef* Quest);
	UFUNCTION(BlueprintPure, Category="Quests") EQuestState GetState(const UQuestDef* Quest) const;
	UFUNCTION(BlueprintPure, Category="Quests") FText CurrentStepText(const UQuestDef* Quest) const;
	UFUNCTION(BlueprintPure, Category="Quests") int32 QuestPoints() const;
	UFUNCTION(BlueprintPure, Category="Quests") const TArray<FQuestProgress>& GetAll() const { return Progress; }

	/** Called by ANpcTalk. Returns true if a quest consumed the conversation (so idle lines are skipped). */
	bool HandleTalk(UNpcDef* Npc);

	FQuestProgress& FindOrAdd(UQuestDef* Quest);

private:
	UFUNCTION() void OnItemGained(UItemDef* Item, int32 Count);
	UFUNCTION() void OnMobKilled(UMobDef* Mob);
	void Advance(FQuestProgress& P);
	void Complete(FQuestProgress& P);
	void Say(UNpcDef* Npc, const TArray<FText>& Lines);
	class AGielinorPlayerState* PS() const;

	UPROPERTY() TArray<FQuestProgress> Progress;
};
