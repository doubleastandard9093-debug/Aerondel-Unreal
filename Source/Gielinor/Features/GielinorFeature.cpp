#include "Features/GielinorFeature.h"
#include "Core/GielinorPlayerState.h"
#include "Core/GielinorEvents.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Bank/BankComponent.h"
#include "Data/GameDatabase.h"
#include "Data/GielinorId.h"
#include "Data/SkillDef.h"
#include "Data/ItemDef.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Misc/DateTime.h"

namespace GielinorFeatureRegistry { static TArray<TWeakObjectPtr<UGielinorFeature>> All; }

void UGielinorFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GielinorFeatureRegistry::All.Add(this);
	ResetState();
}

void UGielinorFeature::Deinitialize()
{
	GielinorFeatureRegistry::All.RemoveAll([this](const TWeakObjectPtr<UGielinorFeature>& P) { return !P.IsValid() || P.Get() == this; });
	Super::Deinitialize();
}

TArray<UGielinorFeature*> UGielinorFeature::AllFor(const UGameInstance* GI)
{
	TArray<UGielinorFeature*> Out;
	for (const TWeakObjectPtr<UGielinorFeature>& P : GielinorFeatureRegistry::All)
		if (UGielinorFeature* F = P.Get()) if (F->GetGameInstance() == GI) Out.Add(F);
	return Out;
}

float UGielinorFeature::TotalXpBonus(const UObject* WorldContext, FName SkillId)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	float B = 0.f;
	if (GI) for (const UGielinorFeature* F : AllFor(GI)) B += F->XpBonus(SkillId);
	return B;
}

int64 UGielinorFeature::NowMs() { return (FDateTime::UtcNow() - FDateTime(1970, 1, 1)).GetTicks() / ETimespan::TicksPerMillisecond; }
FName UGielinorFeature::Id(const FString& WebName) { return GielinorId::FromName(WebName); }

AGielinorPlayerState* UGielinorFeature::GetPS() const
{
	UGameInstance* GI = GetGameInstance();
	UWorld* W = GI ? GI->GetWorld() : nullptr;
	APlayerController* PC = W ? UGameplayStatics::GetPlayerController(W, 0) : nullptr;
	return PC ? PC->GetPlayerState<AGielinorPlayerState>() : nullptr;
}

void UGielinorFeature::Say(const FString& Msg) const
{
	if (UGameInstance* GI = GetGameInstance()) if (UGielinorEvents* Ev = GI->GetSubsystem<UGielinorEvents>()) Ev->Message(FText::FromString(Msg));
}

int32 UGielinorFeature::SkillLevel(const FString& Skill) const { const AGielinorPlayerState* PS = GetPS(); return PS && PS->Skills ? PS->Skills->GetLevelById(Id(Skill)) : 1; }
int32 UGielinorFeature::CombatLevel() const { const AGielinorPlayerState* PS = GetPS(); return PS && PS->Skills ? PS->Skills->CombatLevel() : 3; }

void UGielinorFeature::GiveXp(const FString& Skill, float Xp) const
{
	AGielinorPlayerState* PS = GetPS(); const UGameDatabase* Db = UGameDatabase::Get();
	if (PS && PS->Skills && Db) if (USkillDef* S = Db->FindSkill(Id(Skill))) PS->Skills->AddXp(S, Xp);
}

int32 UGielinorFeature::CountItem(const FString& Item) const
{
	const AGielinorPlayerState* PS = GetPS(); const UGameDatabase* Db = UGameDatabase::Get();
	const UItemDef* I = Db ? Db->FindItem(Id(Item)) : nullptr;
	return PS && I ? PS->Inventory->CountOf(I) : 0;
}

bool UGielinorFeature::TakeItem(const FString& Item, int32 N) const
{
	AGielinorPlayerState* PS = GetPS(); const UGameDatabase* Db = UGameDatabase::Get();
	UItemDef* I = Db ? Db->FindItem(Id(Item)) : nullptr;
	if (!PS || !I || N <= 0 || PS->Inventory->CountOf(I) < N) return N <= 0;
	PS->Inventory->Remove(I, N);
	return true;
}

void UGielinorFeature::GiveItem(const FString& Item, int32 N) const
{
	AGielinorPlayerState* PS = GetPS(); const UGameDatabase* Db = UGameDatabase::Get();
	UItemDef* I = Db ? Db->FindItem(Id(Item)) : nullptr;
	if (!PS || !I || N <= 0) return;
	const int32 Added = PS->Inventory->Add(I, N, true);
	if (Added < N && PS->Bank) PS->Bank->Add(I, N - Added, false);
}

void UGielinorFeature::GiveToBank(const FString& Item, int32 N) const
{
	AGielinorPlayerState* PS = GetPS(); const UGameDatabase* Db = UGameDatabase::Get();
	UItemDef* I = Db ? Db->FindItem(Id(Item)) : nullptr;
	if (PS && PS->Bank && I && N > 0) PS->Bank->Add(I, N, false);
}

int64 UGielinorFeature::JInt(const FJsonObject& O, const TCHAR* Key, int64 Def) { double V = (double)Def; O.TryGetNumberField(Key, V); return (int64)FMath::FloorToDouble(V); }
double UGielinorFeature::JNum(const FJsonObject& O, const TCHAR* Key, double Def) { double V = Def; O.TryGetNumberField(Key, V); return V; }
bool UGielinorFeature::JBool(const FJsonObject& O, const TCHAR* Key) { bool V = false; O.TryGetBoolField(Key, V); return V; }
FString UGielinorFeature::JStr(const FJsonObject& O, const TCHAR* Key) { FString V; O.TryGetStringField(Key, V); return V; }
TSharedPtr<FJsonObject> UGielinorFeature::JObj(const FJsonObject& O, const TCHAR* Key) { const TSharedPtr<FJsonObject>* V = nullptr; return O.TryGetObjectField(Key, V) && V ? *V : nullptr; }

TArray<FString> UGielinorFeature::JStrArr(const FJsonObject& O, const TCHAR* Key)
{
	TArray<FString> Out; const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (O.TryGetArrayField(Key, A) && A) for (const TSharedPtr<FJsonValue>& V : *A) if (V.IsValid() && V->Type == EJson::String) Out.Add(V->AsString());
	return Out;
}

void UGielinorFeature::PutStrArr(FJsonObject& O, const TCHAR* Key, const TArray<FString>& A)
{
	TArray<TSharedPtr<FJsonValue>> Out; for (const FString& S : A) Out.Add(MakeShared<FJsonValueString>(S));
	O.SetArrayField(Key, Out);
}

void UGielinorFeature::PutIntMap(FJsonObject& O, const TCHAR* Key, const TMap<FString, int32>& M)
{
	TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>(); for (const TPair<FString, int32>& P : M) J->SetNumberField(P.Key, P.Value);
	O.SetObjectField(Key, J);
}

TMap<FString, int32> UGielinorFeature::JIntMap(const FJsonObject& O, const TCHAR* Key)
{
	TMap<FString, int32> Out;
	if (TSharedPtr<FJsonObject> J = JObj(O, Key))
		for (const auto& P : J->Values) if (P.Value.IsValid() && P.Value->Type == EJson::Number) Out.Add(FString(*P.Key), (int32)P.Value->AsNumber());
	return Out;
}
