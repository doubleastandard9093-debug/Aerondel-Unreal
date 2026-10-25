#pragma once
#include "CoreMinimal.h"
#include "Interaction/InteractableActor.h"
#include "BankChest.generated.h"

UCLASS()
class GIELINOR_API ABankChest : public AInteractableActor
{
	GENERATED_BODY()
public:
	ABankChest();
	virtual FText GetInteractLabel() const override { return NSLOCTEXT("Gielinor", "Bank", "Bank Bank chest"); }
	virtual void Interact(AActor* InInstigator) override;
	virtual void Tick(float Dt) override;
private:
	TWeakObjectPtr<AActor> User;
};
