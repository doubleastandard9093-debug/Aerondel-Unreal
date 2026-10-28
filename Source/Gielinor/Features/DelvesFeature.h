#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "DelvesFeature.generated.h"

class UMobDef;

/** One spawn group in a delve room. */
USTRUCT(BlueprintType)
struct FDelveSpawn
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Mob;
	UPROPERTY(BlueprintReadOnly) int32 Count = 0;
};

/** Delves of Three: instanced party dungeons (you + two companions), 3 rooms, max 8 foes per room. You pick a role;
 *  inside, the role sets damage reduction, regen and damage. Room spawning is done by the level (GetRoom). */
UCLASS()
class GIELINOR_API UDelvesFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 RoomCap = 8;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("delves"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Clears.Reset(); Best.Reset(); Role = TEXT("dps"); Current = NAME_None; Room = 0; Left = 0; }
	virtual float DamageTakenMult(const UMobDef* Source, float HpFrac) const override;
	virtual float DamageDealtBonus(const UMobDef* Target) const override;
	virtual void OnGameTick() override;

	/** tank | healer | dps */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Delves") bool SetRole(FName NewRole);
	/** sewers | keep. Needs combat level (20 / 35). */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Delves") FString Enter(FName DelveId);
	UFUNCTION(BlueprintPure, Category="Gielinor|Delves") TArray<FDelveSpawn> GetRoom() const;
	UFUNCTION(BlueprintPure, Category="Gielinor|Delves") bool IsInside() const { return !Current.IsNone(); }
	UFUNCTION(BlueprintCallable, Category="Gielinor|Delves") void Leave() { Current = NAME_None; Room = 0; Left = 0; }
private:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	void StartRoom();
	TMap<FString, int32> Clears, Best;
	FName Role = TEXT("dps"), Current;
	int32 Room = 0, Left = 0, Ticks = 0;
	float RegenAcc = 0.f;
};
