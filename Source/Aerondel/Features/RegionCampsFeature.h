#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "RegionCampsFeature.generated.h"

class AAerondelMob;

/** Region mob camps (web regions.js / world.js: world.json regions[].mobs = [type, dx, dz, roam]). Streams like the web:
 *  a region's camp mobs spawn from the normal mob table (drops + respawn intact) when the player comes within its
 *  radius + 40 m and are removed again past radius + 60 m, so the 1,000-level frontier only costs actors where you are. */
UCLASS()
class AERONDEL_API URegionCampsFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	virtual void OnGameTick() override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintPure, Category="Aerondel|Regions") int32 ActiveCampMobs() const;
private:
	struct FCampMob { FString Type; FVector2D Pos; float Roam = 2.f; };
	struct FCamp { FString Name; FVector2D Center; float Radius = 50.f; TArray<FCampMob> Mobs; TArray<TWeakObjectPtr<AAerondelMob>> Live; bool bUp = false; };
	void LoadCamps();
	void Raise(FCamp& C);
	static void Drop(FCamp& C);
	TArray<FCamp> Camps;
	bool bLoaded = false;
};
