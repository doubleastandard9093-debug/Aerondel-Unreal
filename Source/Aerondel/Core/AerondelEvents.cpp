#include "Core/AerondelEvents.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UAerondelEvents* UAerondelEvents::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UAerondelEvents>() : nullptr;
}
