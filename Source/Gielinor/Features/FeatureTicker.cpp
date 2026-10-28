#include "Features/FeatureTicker.h"
#include "Features/GielinorFeature.h"
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
		for (UGielinorFeature* F : UGielinorFeature::AllFor(GI)) F->OnGameTick();
	}
}
