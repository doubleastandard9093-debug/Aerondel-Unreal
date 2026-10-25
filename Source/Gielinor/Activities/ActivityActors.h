#pragma once
#include "CoreMinimal.h"
#include "Interaction/InteractableActor.h"
#include "ActivityActors.generated.h"

class UAgilityCourseDef; class UThieveTargetDef; class URuneAltarDef; class UShopDef; class UItemDef; class UBoxComponent;

/** One agility obstacle. Course + Index pick the data; success moves the player to End (lap bonus on the last one). */
UCLASS()
class GIELINOR_API AAgilityObstacle : public AInteractableActor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Agility") FName CourseId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Agility") int32 Index = 0;
	/** Where the player lands (relative, drag the diamond in the viewport). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Agility", meta=(MakeEditWidget=true)) FVector End = FVector(400, 0, 0);
	virtual FText GetInteractLabel() const override;
	virtual void Interact(AActor* InInstigator) override;
	/** Desktop/VR presentation hook (play the crossing animation); the move itself is already done. */
	UFUNCTION(BlueprintImplementableEvent, Category="Agility") void OnCrossed(AActor* Player, bool bSuccess);
private:
	const struct FAgilityObstacle* Obstacle(const UAgilityCourseDef*& OutCourse) const;
};

/** Pickpocket target (an NPC or stall). */
UCLASS()
class GIELINOR_API AThieveTarget : public AInteractableActor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Thieving") FName TargetId;
	virtual FText GetInteractLabel() const override;
	virtual void Interact(AActor* InInstigator) override;
};

/** Rune altar: crafts every essence in the backpack at once. */
UCLASS()
class GIELINOR_API ARuneAltar : public AInteractableActor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Runecrafting") FName AltarId;
	virtual FText GetInteractLabel() const override;
	virtual void Interact(AActor* InInstigator) override;
};

/** Altar in a church: recharges prayer points. */
UCLASS()
class GIELINOR_API APrayerAltar : public AInteractableActor
{
	GENERATED_BODY()
public:
	virtual FText GetInteractLabel() const override;
	virtual void Interact(AActor* InInstigator) override;
};

class AShopCounter;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShopOpened, AShopCounter*, Shop, AActor*, Player);

/** Shop keeper / counter. Interact opens the shop (UI binds OnShopOpened); Buy and Sell do the trade. */
UCLASS()
class GIELINOR_API AShopCounter : public AInteractableActor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shop") FName ShopId;
	UPROPERTY(BlueprintAssignable) FOnShopOpened OnShopOpened;
	virtual FText GetInteractLabel() const override;
	virtual void Interact(AActor* InInstigator) override;
	UFUNCTION(BlueprintPure, Category="Shop") UShopDef* GetShop() const;
	/** Stock entries this player may see (quest / 99 / quest cape unlocks). */
	UFUNCTION(BlueprintPure, Category="Shop") TArray<int32> VisibleStock(AActor* Player) const;
	UFUNCTION(BlueprintPure, Category="Shop") int32 PriceOf(AActor* Player, int32 StockIndex) const;
	UFUNCTION(BlueprintCallable, Category="Shop") bool Buy(AActor* Player, int32 StockIndex, int32 Amount = 1);
	UFUNCTION(BlueprintCallable, Category="Shop") bool Sell(AActor* Player, UItemDef* SoldItem, int32 Amount = 1);
};

/** Invisible volume for "visit" quest steps and named places. */
UCLASS()
class GIELINOR_API AQuestPlaceVolume : public AActor
{
	GENERATED_BODY()
public:
	AQuestPlaceVolume();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Place") TObjectPtr<UBoxComponent> Box;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place") FName PlaceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place") FText DisplayName;
protected:
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
};
