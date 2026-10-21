#include "Core/GielinorEvents.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UGielinorEvents* UGielinorEvents::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UGielinorEvents>() : nullptr;
}
