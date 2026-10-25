#pragma once
#include "CoreMinimal.h"
#include "Interaction/InteractableActor.h"
#include "NpcTalk.generated.h"

class UNpcDef;

UCLASS()
class GIELINOR_API ANpcTalk : public AInteractableActor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") TObjectPtr<UNpcDef> Definition;
	virtual FText GetInteractLabel() const override;
	virtual void Interact(AActor* InInstigator) override;
	virtual void OnConstruction(const FTransform& Xf) override;
private:
	int32 LineIndex = 0;
};
