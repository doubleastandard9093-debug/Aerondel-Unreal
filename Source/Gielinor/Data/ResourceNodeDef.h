#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/GielinorTypes.h"
#include "ResourceNodeDef.generated.h"

UCLASS(BlueprintType)
class GIELINOR_API UResourceNodeDef : public UPrimaryDataAsset
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

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("ResourceNode"), GetFName()); }
};
