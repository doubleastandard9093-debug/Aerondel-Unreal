#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "Features/CompanionsFeature.h"
#include "SagaFeature.generated.h"

class UMobDef;

/** The Wanderer's Saga (Phase 3): Chronicler Corwin's six-chapter story. Pick a calling (+5% XP in its skills), clear each
 *  chapter's objective (kills or items), answer on the choice wheel; flags come back later (mercy, Varro, Emberfold discount),
 *  chapter 4 tests your follower's bond, and the finale title follows your most-picked tone. */
UCLASS()
class AERONDEL_API USagaFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("saga"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Calling = NAME_None; Chapter = 0; bActive = false; Prog = 0; Flags.Reset(); Choices.Reset(); Title.Reset(); }
	virtual float XpBonus(FName SkillId) const override;

	static int32 NumChapters();
	/** warden | arcanist | ranger */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Saga") bool ChooseCalling(FName NewCalling);
	/** Empty if the chapter can start, otherwise why not (total level / quest points gate). */
	UFUNCTION(BlueprintPure, Category="Aerondel|Saga") FString GateText() const;
	/** Talk to Corwin: returns the lines to show. Starts the chapter if allowed. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Saga") TArray<FString> Talk();
	/** True when the objective is met and the wheel (or finale) is waiting. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Saga") bool ReadyToFinish() const;
	/** Answer the chapter's wheel (finale ignores the tone and computes the ending). Returns the reply line. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Saga") FString Finish(EWheelTone Tone);
	UFUNCTION(BlueprintPure, Category="Aerondel|Saga") FString ObjectiveText() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Saga") int32 Need() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Saga") int32 Progress() const { return Prog; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Saga") bool HasFlag(const FString& F) const { return Flags.Contains(F); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Saga") FString GetTitle() const { return Title; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Saga") int32 ChaptersDone() const { return Chapter; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Saga") EWheelTone EndingTone() const;
	void SetFlag(const FString& F) { Flags.AddUnique(F); }

private:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	FName Calling;
	int32 Chapter = 0, Prog = 0;
	bool bActive = false;
	TArray<FString> Flags;
	TArray<TPair<int32, EWheelTone>> Choices;
	FString Title;
};
