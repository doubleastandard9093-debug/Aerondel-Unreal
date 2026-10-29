#pragma once
#include "CoreMinimal.h"
#include "Tickable.h"
#include "Features/GielinorFeature.h"
#include "SailingFeature.generated.h"

/** One boat in the Sound (yours or a Drowned Crew ship). Positions are web metres (x east, z = UE Y); Yaw in radians, 0 = +z. */
USTRUCT(BlueprintType)
struct FSeaBoat
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) float X = 0.f;
	UPROPERTY(BlueprintReadOnly) float Z = 0.f;
	UPROPERTY(BlueprintReadOnly) float Yaw = 0.f;
	UPROPERTY(BlueprintReadOnly) float Speed = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 Hp = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaxHp = 0;
	UPROPERTY(BlueprintReadOnly) int32 Guns = 2;
	/** 0 = sloop, 1 = brig (enemies); hull tier 1-3 for yours. */
	UPROPERTY(BlueprintReadOnly) int32 Kind = 0;
	float Reload = 0.f;
};

/** A live cannonball (for the visual pool). */
USTRUCT(BlueprintType)
struct FCannonball
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FVector Pos = FVector::ZeroVector; // web metres, Y up
	FVector Vel = FVector::ZeroVector;
	float Left = 0.f;
	int32 Dmg = 0;
	bool bMine = false;
};

USTRUCT(BlueprintType)
struct FTreasureChart
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) float X = 0.f;
	UPROPERTY(BlueprintReadOnly) float Z = 0.f;
	UPROPERTY(BlueprintReadOnly) FName Island;
	UPROPERTY(BlueprintReadOnly) int32 Tier = 1;
};

USTRUCT(BlueprintType)
struct FSeaIsland
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) float X = 0.f;
	UPROPERTY(BlueprintReadOnly) float Z = 0.f;
	UPROPERTY(BlueprintReadOnly) float R = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bPort = false;
	UPROPERTY(BlueprintReadOnly) FVector2D Dock = FVector2D::ZeroVector;
};

/** Sails of Sarim: the Sarim Sound south of the world (z > 760), its three islands, your ship (Sloop / Brigantine / Galleon,
 *  built and upgraded by Halvard the Shipwright on the Port Sarim dock), deckhands (-8% reload each, max 6), sworn-companion
 *  officers (surgeon / lookout / gunnery / helm, only while they travel with you), Drowned Crew ships with broadside cannon
 *  fights, and Treasure charts dug up with a Spade. Simulated per frame here in web metres; a Blueprint draws the ship, the
 *  enemy ships and the cannonball pool from GetShip / GetEnemies / GetBalls (Low tier: hull mesh only, no sails cloth, no wake). */
UCLASS()
class GIELINOR_API USailingFeature : public UGielinorFeature, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual FString GetSaveKey() const override { return TEXT("vessel"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override;

	// FTickableGameObject
	virtual void Tick(float Dt) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USailingFeature, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate() && (bAtSea || !Moored.IsNone()); }
	virtual ETickableTickType GetTickableTickType() const override { return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr; }

	static const TArray<FSeaIsland>& Islands();

	// ---- Shipwright (Halvard, Port Sarim dock) ----
	UFUNCTION(BlueprintCallable, Category="Gielinor|Sailing") FString BuildOrUpgrade();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Sailing") FString PatchHull();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Sailing") FString HireDeckhand();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Sailing") FString ToggleOfficer(FName CompanionId);
	UFUNCTION(BlueprintPure, Category="Gielinor|Sailing") FString ShipwrightLine() const;

	// ---- At sea ----
	UFUNCTION(BlueprintCallable, Category="Gielinor|Sailing") bool SetSail();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Sailing") bool Reboard();
	/** Tap the sea: steer toward a web position at full sail. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Sailing") bool SteerTo(FVector2D WebPos);
	/** Helm input each frame: Turn -1..1 (A/D, left stick X), Throttle 1 full / 0 strike sails / <0 unchanged. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Sailing") void Helm(float Turn, float Throttle);
	/** Broadside: bPort = left side (Z / Port button), else starboard (X). */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Sailing") void Fire(bool bPort);

	// ---- Treasure charts ----
	UFUNCTION(BlueprintCallable, Category="Gielinor|Sailing") bool ReadChart();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Sailing") bool DigHere();

	UFUNCTION(BlueprintPure, Category="Gielinor|Sailing") bool IsAtSea() const { return bAtSea; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Sailing") FName GetMoored() const { return Moored; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Sailing") int32 GetHull() const { return Hull; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Sailing") FSeaBoat GetShip() const { return Ship; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Sailing") TArray<FSeaBoat> GetEnemies() const { return Enemies; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Sailing") TArray<FCannonball> GetBalls() const { return Balls; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Sailing") TArray<FTreasureChart> GetCharts() const { return Charts; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Sailing") float ReloadLeft(bool bPort) const { return bPort ? ReloadPort : ReloadStar; }
	/** Sea-trip tallies: wins and losses at the Brineholt card tables live here too (vessel.cards in the web save). */
	void AddCardResult(bool bWon) { (bWon ? CardsWon : CardsLost)++; }

	static bool InSound(float X, float Z) { return Z > 760.f; }

private:
	struct FHullDef { const TCHAR* Name; int32 Hp; float Speed; int32 Guns; int32 Cost; int32 SmithReq; int32 CraftReq; const TCHAR* Bar; int32 Bars; const TCHAR* Logs; int32 LogN; };
	static const FHullDef& HullDef(int32 N);
	bool Officer(const TCHAR* Trait) const;
	const FSeaIsland* IslandAt(float X, float Z, float Pad = 0.f) const;
	bool MoveBoat(FSeaBoat& B, float Dt, float Speed, float TurnTo) const;
	void SpawnEnemy();
	void SinkEnemy(int32 Index);
	void HitShip(int32 Dmg);
	void GoAshore(const FSeaIsland& Isl);
	void HomeToSarim(const FString& Msg);
	void PlacePawn(float X, float Z, float Y, float Yaw) const;
	FCannonball* NewBall();

	// Saved
	int32 Hull = 0, HullHp = 0, Crew = 0, Sunk = 0, Sank = 0, Drowned = 0, Dug = 0, CardsWon = 0, CardsLost = 0;
	TArray<FName> Officers;
	TArray<FTreasureChart> Charts;
	// Runtime
	bool bAtSea = false;
	FName Moored;
	FSeaBoat Ship;
	TArray<FSeaBoat> Enemies;
	TArray<FCannonball> Balls;
	bool bHasTarget = false;
	FVector2D Target = FVector2D::ZeroVector;
	float Throttle = 0.f, TurnInput = 0.f, ReloadPort = 0.f, ReloadStar = 0.f, NextSpawn = 0.f, RepairT = 0.f, T = 0.f;
};
