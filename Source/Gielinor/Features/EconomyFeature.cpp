#include "Features/EconomyFeature.h"
#include "Core/GielinorPlayerState.h"
#include "Inventory/EquipmentComponent.h"
#include "Data/GameDatabase.h"
#include "Data/ContentDefs.h"
#include "Data/ItemDef.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

UEconomyFeature* UEconomyFeature::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UEconomyFeature>() : nullptr;
}

FString UEconomyFeature::Key(FName ShopId, const UItemDef* Item) { return ShopId.ToString() + TEXT(":") + (Item ? Item->Id.ToString() : FString()); }

int32 UEconomyFeature::StockCap(int32 P) { return P <= 10 ? 500 : P <= 100 ? 100 : P <= 1000 ? 25 : P <= 20000 ? 6 : 2; }

bool UEconomyFeature::IsTopTier(const UItemDef* It)
{
	if (!It || It->EquipSlot == EEquipSlot::None || It->ToolType != EToolType::None) return false;
	if (It->bUnique) return true;
	if (It->RequiredLevel >= 40) return true;
	for (const auto& R : It->SkillRequirements) if (R.Value >= 40) return true;
	return false;
}

int32 UEconomyFeature::StockLeft(FName ShopId, const FShopStock& E, int32 Base) const
{
	if (!E.Item || E.Item->ToolType != EToolType::None) return MAX_int32;
	const float* B = Bought.Find(Key(ShopId, E.Item));
	return FMath::Max(0, StockCap(Base) - FMath::CeilToInt(B ? *B : 0.f));
}

int32 UEconomyFeature::UnitPrice(FName ShopId, const FShopStock& E, int32 Base, int32 SellBack) const
{
	const bool bTool = E.Item && E.Item->ToolType != EToolType::None;
	const float* B = bTool ? nullptr : Bought.Find(Key(ShopId, E.Item));
	const int32 Climbed = FMath::CeilToInt(Base * (1.f + PriceClimb * FMath::Min(1.f, (B ? *B : 0.f) / StockCap(Base))));
	return FMath::Max(Climbed, FMath::CeilToInt(SellBack * ArbMargin));
}

void UEconomyFeature::NoteBought(FName ShopId, const UItemDef* Item, int32 N)
{
	if (!Item || Item->ToolType != EToolType::None) return;
	Bought.FindOrAdd(Key(ShopId, Item)) += N;
}

void UEconomyFeature::OnGameTick()
{
	const UGameDatabase* Db = UGameDatabase::Get();
	for (auto It = Bought.CreateIterator(); It; ++It)
	{
		int32 Cap = 10;
		FString ShopS, ItemS;
		if (Db && It.Key().Split(TEXT(":"), &ShopS, &ItemS))
			if (const UShopDef* Sh = Db->FindShop(FName(*ShopS)))
				for (const FShopStock& E : Sh->Stock)
					if (E.Item && E.Item->Id == FName(*ItemS)) { Cap = StockCap(E.Price > 0 ? E.Price : FMath::Max(1, E.Item->Value)); break; }
		It.Value() -= (float)Cap / RestockTicks;
		if (It.Value() <= 0.f) It.RemoveCurrent();
	}
}

void UEconomyFeature::OnPlayerHit()
{
	const AGielinorPlayerState* PS = GetPS();
	if (!PS || !PS->Equipment) return;
	for (const auto& P : PS->Equipment->GetAll())
	{
		const UItemDef* It = P.Value;
		if (!IsTopTier(It)) continue;
		float& W = Wear.FindOrAdd(It->Id);
		const float Was = W;
		W = FMath::Min(100.f, W + WearPerHit);
		if (Was < 100.f && W >= 100.f) Say(FString::Printf(TEXT("Your %s is badly worn and loses some of its strength. Repair it at any shop."), *It->DisplayName.ToString()));
	}
}

float UEconomyFeature::WearOf(const UItemDef* Item) const { const float* W = Item ? Wear.Find(Item->Id) : nullptr; return W ? *W : 0.f; }
float UEconomyFeature::WearMult(const UItemDef* Item) const { return WearOf(Item) >= 100.f ? WornMult : 1.f; }

int32 UEconomyFeature::RepairCost() const
{
	const UGameDatabase* Db = UGameDatabase::Get();
	int32 C = 0;
	for (const auto& P : Wear) if (const UItemDef* It = Db ? Db->FindItem(P.Key) : nullptr) C += FMath::CeilToInt(It->Value * RepairRate * P.Value);
	return C;
}

FString UEconomyFeature::RepairAll()
{
	const int32 C = RepairCost();
	if (!C) return TEXT("Nothing needs repairing.");
	if (!TakeCoins(C)) return FString::Printf(TEXT("Repairs cost %d coins. You don't have enough."), C);
	Wear.Reset();
	return FString::Printf(TEXT("Your gear is repaired for %d coins."), C);
}

void UEconomyFeature::OnPlayerDied()
{
	const int32 Fee = FMath::Min(DeathFeeCap, FMath::FloorToInt(CountItem(TEXT("Coins")) * DeathFee));
	if (Fee > 0 && TakeCoins(Fee)) Say(FString::Printf(TEXT("The healers charge %d coins."), Fee));
}

void UEconomyFeature::SaveState(FJsonObject& Out) const
{
	TSharedPtr<FJsonObject> S = MakeShared<FJsonObject>(), W = MakeShared<FJsonObject>();
	for (const auto& P : Bought) S->SetNumberField(P.Key, P.Value);
	for (const auto& P : Wear) W->SetNumberField(P.Key.ToString(), P.Value);
	Out.SetObjectField(TEXT("shopState"), S);
	Out.SetObjectField(TEXT("wear"), W);
}

void UEconomyFeature::LoadState(const FJsonObject& In)
{
	ResetState();
	if (TSharedPtr<FJsonObject> S = JObj(In, TEXT("shopState")))
		for (const auto& P : S->Values) { const double N = P.Value->AsNumber(); const FString K(*P.Key); if (K.Contains(TEXT(":")) && N > 0) Bought.Add(K, FMath::Min(9999.0, N)); }
	if (TSharedPtr<FJsonObject> W = JObj(In, TEXT("wear")))
		for (const auto& P : W->Values) { const double N = P.Value->AsNumber(); if (N > 0) Wear.Add(FName(*P.Key), FMath::Min(100.0, N)); }
}
