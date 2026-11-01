#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NpcDef.generated.h"

UCLASS(BlueprintType)
class GIELINOR_API UNpcDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") FLinearColor Tint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") TArray<FText> IdleLines;
	/** Feature NPCs (v18 port): what talking opens - shipwright, hexer, saga, roost, hearth, charter, siege, tasks, arena, quirks,
	 *  writs, delves, paths, harbour, tides, crown, travel (town waystone), say, or shop:<ShopId>. None = plain quest/idle NPC. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") FName Service;
	/** Where the world blockout stands them, web metres (x, z). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") FVector2D Home = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") bool bHasHome = false;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Npc"), GetFName()); }
};
