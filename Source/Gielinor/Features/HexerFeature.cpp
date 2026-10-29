#include "Features/HexerFeature.h"
#include "Core/GielinorPlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "Data/ItemDef.h"

TArray<FString> UHexerFeature::GrimyHerbsHeld() const
{
	TArray<FString> Out;
	if (const AGielinorPlayerState* PS = GetPS())
		if (PS->Inventory)
			for (const FItemStack& S : PS->Inventory->GetSlots())
				if (!S.IsEmpty())
				{
					const FString N = S.Item->DisplayName.ToString();
					if (N.StartsWith(TEXT("Grimy "))) Out.AddUnique(N);
				}
	return Out;
}

FString UHexerFeature::Brew(bool bBone)
{
	const int32 Ess = CountItem(TEXT("Rune essence")) + CountItem(TEXT("Pure essence"));
	const TArray<FString> Herbs = GrimyHerbsHeld();
	int32 Src = 0;
	if (bBone) Src = CountItem(TEXT("Bones"));
	else for (const FString& H : Herbs) Src += CountItem(H);
	const int32 N = FMath::Min3(Ess, Src, 10);
	if (N <= 0) return bBone ? TEXT("Bring me Bones and Rune essence, child.") : TEXT("Bring me grimy herbs from the bog and Rune essence.");
	int32 HerbIdx = 0;
	for (int32 K = 0; K < N; ++K)
	{
		if (!TakeItem(TEXT("Rune essence"), 1)) TakeItem(TEXT("Pure essence"), 1);
		if (bBone) TakeItem(TEXT("Bones"), 1);
		else
		{
			while (HerbIdx < Herbs.Num() && CountItem(Herbs[HerbIdx]) <= 0) ++HerbIdx;
			if (HerbIdx < Herbs.Num()) TakeItem(Herbs[HerbIdx], 1);
		}
	}
	const TCHAR* Rune = bBone ? TEXT("Bone rune") : TEXT("Bog rune");
	const float Xp = bBone ? 9.f : 6.f;
	GiveItem(Rune, N * 3);
	GiveXp(TEXT("Runecrafting"), N * Xp);
	GiveXp(TEXT("Magic"), N * Xp * 0.5f);
	return FString::Printf(TEXT("There: %d %ss. Hexes take from the bog and the grave; mind what you curse."), N * 3, Rune);
}
