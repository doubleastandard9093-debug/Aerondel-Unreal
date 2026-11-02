#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/AerondelTypes.h"
#include "ResourceNodeDef.generated.h"

/** Fishing spots: tried best-first; the web build gives non-last catches a 60% weight. */
USTRUCT(BlueprintType)
struct FResourceCatch
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<class UItemDef> Item = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Xp = 10.f;
};

UCLASS(BlueprintType)
class AERONDEL_API UResourceNodeDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") EResourceKind Kind = EResourceKind::Tree;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") TObjectPtr<class USkillDef> Skill = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") int32 LevelRequired = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") EToolType ToolRequired = EToolType::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") float Xp = 25.f;
	/** Base success chance per gather tick at the level requirement (rises with level and tool tier). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") float BaseSuccess = 0.45f;
	/** Chance the node depletes after a successful gather (1 = always, 0 = never e.g. fishing spots). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") float DepleteChance = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") float RespawnSeconds = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") float TickSeconds = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") TObjectPtr<class UItemDef> Product = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") FLinearColor Tint = FLinearColor::White;
	/** If non-empty, overrides Product/Xp/LevelRequired (fishing spots with several fish). Highest level first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") TArray<FResourceCatch> Catches;
	/** Essence rock style: at PureLevel+ gives PureProduct instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") int32 PureLevel = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Node") TObjectPtr<class UItemDef> PureProduct = nullptr;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("ResourceNode"), GetFName()); }
};
