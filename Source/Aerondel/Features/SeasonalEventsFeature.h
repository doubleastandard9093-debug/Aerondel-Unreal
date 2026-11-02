#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "SeasonalEventsFeature.generated.h"

class UMobDef;
class USkillDef;

/** Seasonal events (web js/events.js): the real-world season's festival plus the specials Lantern Night (10/24-11/7) and
 *  Founders' Week (6/1-6/10). Kills drop tokens (15%; specials 30% and two at a time), Festival Herald Odelyn in Larkhaven
 *  hands out a daily task and runs a token stall and a 25-token Blessing. Save: {daily{day,kind,skill,n,base,c,done}, seen, total}. */
UCLASS()
class AERONDEL_API USeasonalEventsFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("events"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override;
	virtual void OnGameTick() override;

	/** Indexes into the static event table for the active events today. */
	static TArray<int32> ActiveIndices();
	static int32 NumEvents();
	static FString EventName(int32 I);
	static FString EventToken(int32 I);
	static FString EventDeco(int32 I);
	static bool IsSpecial(int32 I);
	/** Number of stall items for an event. */
	static int32 NumShopItems(int32 I);

	UFUNCTION(BlueprintPure, Category="Aerondel|Events") TArray<FString> ActiveEventIds() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Events") FString Greeting() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Events") FString TaskText();
	UFUNCTION(BlueprintCallable, Category="Aerondel|Events") FString TurnInTask();
	UFUNCTION(BlueprintPure, Category="Aerondel|Events") FString ShopLabel(int32 EventIndex, int32 ItemIndex) const;
	UFUNCTION(BlueprintCallable, Category="Aerondel|Events") FString Buy(int32 EventIndex, int32 ItemIndex);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Events") FString Blessing(int32 EventIndex);
	UFUNCTION(BlueprintPure, Category="Aerondel|Events") int32 TokenCount(int32 EventIndex) const { return CountItem(EventToken(EventIndex)); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Events") int32 TasksDone() const { return Total; }
private:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	UFUNCTION() void HandleXp(USkillDef* Skill, float Amount, float TotalXp, int32 Level);
	void EnsureDaily();
	/** Shows only the running events' festival decor (actor tags festival:<id>); festival_full pieces hide on Low/Quest. */
	void RefreshDecor();
	int32 DecorTicks = 0;
	FString WeakestSkill() const;
	static FString Today();
	// daily task
	FString Day, Kind, TaskSkill; int32 N = 0, C = 0; double Base = 0.0; bool bDone = false;
	TArray<FString> Seen; int32 Total = 0; bool bAnnounced = false;
};
