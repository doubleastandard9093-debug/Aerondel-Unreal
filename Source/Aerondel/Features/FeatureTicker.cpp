#include "Features/FeatureTicker.h"
#include "Features/AerondelFeature.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UFeatureTicker::Tick(float DeltaTime)
{
	UGameInstance* GI = GetGameInstance();
	const UWorld* W = GI ? GI->GetWorld() : nullptr;
	if (!W || !W->IsGameWorld()) return;
	Acc += FMath::Min(DeltaTime, 1.f);
	while (Acc >= 0.6f)
	{
		Acc -= 0.6f;
		for (UAerondelFeature* F : UAerondelFeature::AllFor(GI)) F->OnGameTick();
	}
}
