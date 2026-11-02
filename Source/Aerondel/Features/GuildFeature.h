#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "GuildFeature.generated.h"

class UMobDef;

/** Player guild (web js/guild.js): charter with Guildmaster Halvard in Larkhaven (5,000 coins), treasury, renown, six hall rooms
 *  (levels 0-5), timed guild boosts, hired adventurers and guild crests. The hall stands west of Larkhaven (web -22, 18).
 *  Save: null or {name, renown, funds, rooms, hired, boost{id, until}, xpAcc}. */
UCLASS()
class AERONDEL_API UGuildFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("guild"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override;
	virtual float XpBonus(FName SkillId) const override;
	virtual float DamageTakenMult(const UMobDef* Source, float HpFrac) const override;

	static TArray<FString> CharterNames();
	static int32 NumRooms();
	static int32 NumBoosts();
	UFUNCTION(BlueprintPure, Category="Aerondel|Guild") bool HasGuild() const { return !Name.IsEmpty(); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Guild") FString Status() const;
	UFUNCTION(BlueprintCallable, Category="Aerondel|Guild") FString Found(int32 NameIndex);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Guild") FString Deposit(int32 Coins);
	UFUNCTION(BlueprintPure, Category="Aerondel|Guild") FString RoomLabel(int32 Index) const;
	UFUNCTION(BlueprintCallable, Category="Aerondel|Guild") FString UpgradeRoom(int32 Index);
	UFUNCTION(BlueprintPure, Category="Aerondel|Guild") FString BoostLabel(int32 Index) const;
	UFUNCTION(BlueprintCallable, Category="Aerondel|Guild") FString RaiseBoost(int32 Index);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Guild") FString Hire();
	UFUNCTION(BlueprintCallable, Category="Aerondel|Guild") FString TurnInCrests();
	UFUNCTION(BlueprintPure, Category="Aerondel|Guild") int32 GetRoom(FName Room) const { return Rooms.FindRef(Room.ToString()); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Guild") FString ActiveBoost() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Guild") TArray<FString> Members() const;
	static FVector2D Hall() { return FVector2D(-22.f, 18.f); }
private:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	UFUNCTION() void HandleXp(class USkillDef* Skill, float Amount, float TotalXp, int32 Level);
	void AddRenown(int32 N);
	int32 Seats() const { return 4 + Rooms.FindRef(TEXT("hall")) * 3; }
	FString Name;
	int32 Renown = 0, Funds = 0, XpAcc = 0;
	TMap<FString, int32> Rooms;
	TArray<FString> Hired;
	FString BoostId; int64 BoostUntil = 0;
};
