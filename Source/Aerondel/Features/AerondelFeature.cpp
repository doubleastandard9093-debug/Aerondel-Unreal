#include "Features/AerondelFeature.h"
#include "Core/AerondelPlayerState.h"
#include "Core/AerondelEvents.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Bank/BankComponent.h"
#include "Data/GameDatabase.h"
#include "Data/AerondelId.h"
#include "Data/SkillDef.h"
#include "Data/ItemDef.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Misc/DateTime.h"
#include "GameFramework/Pawn.h"
#include "Combat/HealthComponent.h"
#include "Core/PlayerStatsComponent.h"
#include "Features/FlowFeature.h"
#include "Combat/AerondelMob.h"
#include "Data/MobDef.h"
#include "UObject/Package.h"
#include "CollisionQueryParams.h"

namespace AerondelFeatureRegistry { static TArray<TWeakObjectPtr<UAerondelFeature>> All; }

void UAerondelFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	AerondelFeatureRegistry::All.Add(this);
	ResetState();
}

void UAerondelFeature::Deinitialize()
{
	AerondelFeatureRegistry::All.RemoveAll([this](const TWeakObjectPtr<UAerondelFeature>& P) { return !P.IsValid() || P.Get() == this; });
	Super::Deinitialize();
}

TArray<UAerondelFeature*> UAerondelFeature::AllFor(const UGameInstance* GI)
{
	TArray<UAerondelFeature*> Out;
	for (const TWeakObjectPtr<UAerondelFeature>& P : AerondelFeatureRegistry::All)
		if (UAerondelFeature* F = P.Get()) if (F->GetGameInstance() == GI) Out.Add(F);
	return Out;
}

float UAerondelFeature::TotalXpBonus(const UObject* WorldContext, FName SkillId)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	float B = 0.f;
	if (GI) for (const UAerondelFeature* F : AllFor(GI)) B += F->XpBonus(SkillId);
	return B;
}

static const UGameInstance* FeatureGI(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetGameInstance() : nullptr;
}

float UAerondelFeature::TotalDamageTakenMult(const UObject* WorldContext, const UMobDef* Source, float HpFrac)
{
	float M = 1.f;
	if (const UGameInstance* GI = FeatureGI(WorldContext)) for (const UAerondelFeature* F : AllFor(GI)) M *= F->DamageTakenMult(Source, HpFrac);
	return FMath::Clamp(M, 0.1f, 3.f);
}

float UAerondelFeature::TotalDamageDealtBonus(const UObject* WorldContext, const UMobDef* Target)
{
	float B = 0.f;
	if (const UGameInstance* GI = FeatureGI(WorldContext)) for (const UAerondelFeature* F : AllFor(GI)) B += F->DamageDealtBonus(Target);
	return FMath::Clamp(B, -0.5f, 2.f);
}

void UAerondelFeature::NotifyPlayerDied(const UObject* WorldContext)
{
	if (const UGameInstance* GI = FeatureGI(WorldContext)) for (UAerondelFeature* F : AllFor(GI)) F->OnPlayerDied();
}

int64 UAerondelFeature::NowMs() { return (FDateTime::UtcNow() - FDateTime(1970, 1, 1)).GetTicks() / ETimespan::TicksPerMillisecond; }
FName UAerondelFeature::Id(const FString& WebName) { return AerondelId::FromName(WebName); }

AAerondelPlayerState* UAerondelFeature::GetPS() const
{
	UGameInstance* GI = GetGameInstance();
	UWorld* W = GI ? GI->GetWorld() : nullptr;
	APlayerController* PC = W ? UGameplayStatics::GetPlayerController(W, 0) : nullptr;
	return PC ? PC->GetPlayerState<AAerondelPlayerState>() : nullptr;
}

bool UAerondelFeature::PlayerWebPos(FVector2D& Out) const
{
	const AAerondelPlayerState* PS = GetPS(); const APawn* P = PS ? PS->GetPawn() : nullptr;
	if (!P) return false;
	const FVector L = P->GetActorLocation(); Out = FVector2D(L.X / 100.0, L.Y / 100.0); return true;
}

int32 UAerondelFeature::HurtPlayer(float Raw, const FString& Why) const
{
	AAerondelPlayerState* PS = GetPS();
	if (!PS || !PS->Health || !PS->Stats || PS->Health->IsDead() || Raw <= 0.f) return 0;
	if (const UFlowFeature* F = Feature<UFlowFeature>()) if (F->IsDodging()) { Say(FString::Printf(TEXT("You roll clear of %s."), *Why.ToLower())); return 0; }
	const int32 Before = PS->Health->GetCurrent();
	PS->Stats->ApplyPlayerDamage(FMath::Max(1, FMath::RoundToInt(Raw)), nullptr); // feature damage mults + death handling
	const int32 D = Before - PS->Health->GetCurrent();
	if (D > 0) Say(FString::Printf(TEXT("%s hits you for %d."), *Why, D));
	return D;
}

static FVector GroundAt(UWorld* W, FVector2D WebPos, float Lift)
{
	const FVector Top = UAerondelFeature::WebToWorld(WebPos.X, WebPos.Y, 200.0), Bottom = UAerondelFeature::WebToWorld(WebPos.X, WebPos.Y, -200.0);
	FHitResult Hit;
	if (W && W->LineTraceSingleByChannel(Hit, Top, Bottom, ECC_Visibility)) return Hit.ImpactPoint + FVector(0, 0, Lift);
	return UAerondelFeature::WebToWorld(WebPos.X, WebPos.Y) + FVector(0, 0, Lift);
}

AAerondelMob* UAerondelFeature::SpawnTunedMob(const FString& Base, FVector2D WebPos, const FString& Name, int32 Hp, int32 MaxHit, int32 Att, int32 Def, int32 Level, float Scale, bool bBoss) const
{
	UGameInstance* GI = GetGameInstance(); UWorld* W = GI ? GI->GetWorld() : nullptr;
	const UGameDatabase* Db = UGameDatabase::Get();
	UMobDef* BaseDef = Db ? Db->FindMob(Id(Base)) : nullptr;
	if (!W || !BaseDef) { UE_LOG(LogTemp, Warning, TEXT("[Aerondel] SpawnTunedMob: no world or mob '%s'"), *Base); return nullptr; }
	UMobDef* D = DuplicateObject<UMobDef>(BaseDef, GetTransientPackage());
	D->DisplayName = FText::FromString(Name); D->Hitpoints = Hp; D->MaxHit = MaxHit; D->AttackLevel = Att; D->DefenceLevel = Def; D->CombatLevel = Level;
	D->bAggressive = true; D->AggroRadius = FMath::Max(D->AggroRadius, 1200.f); D->RespawnSeconds = 1.0e6f; D->Drops.Reset(); D->bBoss = bBoss;
	D->BodySize *= Scale;
	const FTransform Xf(FRotator::ZeroRotator, GroundAt(W, WebPos, D->BodySize.Z * 0.5f + 10.f));
	AAerondelMob* M = W->SpawnActorDeferred<AAerondelMob>(AAerondelMob::StaticClass(), Xf, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!M) return nullptr;
	M->Definition = D;
	M->FinishSpawning(Xf);
	return M;
}

void UAerondelFeature::TeleportPlayer(FVector2D WebPos) const
{
	const AAerondelPlayerState* PS = GetPS(); APawn* P = PS ? PS->GetPawn() : nullptr;
	if (P) P->TeleportTo(GroundAt(P->GetWorld(), WebPos, 100.f), P->GetActorRotation());
}

void UAerondelFeature::Say(const FString& Msg) const
{
	if (UGameInstance* GI = GetGameInstance()) if (UAerondelEvents* Ev = GI->GetSubsystem<UAerondelEvents>()) Ev->Message(FText::FromString(Msg));
}

int32 UAerondelFeature::SkillLevel(const FString& Skill) const { const AAerondelPlayerState* PS = GetPS(); return PS && PS->Skills ? PS->Skills->GetLevelById(Id(Skill)) : 1; }
int32 UAerondelFeature::CombatLevel() const { const AAerondelPlayerState* PS = GetPS(); return PS && PS->Skills ? PS->Skills->CombatLevel() : 3; }

void UAerondelFeature::GiveXp(const FString& Skill, float Xp) const
{
	AAerondelPlayerState* PS = GetPS(); const UGameDatabase* Db = UGameDatabase::Get();
	if (PS && PS->Skills && Db) if (USkillDef* S = Db->FindSkill(Id(Skill))) PS->Skills->AddXp(S, Xp);
}

int32 UAerondelFeature::CountItem(const FString& Item) const
{
	const AAerondelPlayerState* PS = GetPS(); const UGameDatabase* Db = UGameDatabase::Get();
	const UItemDef* I = Db ? Db->FindItem(Id(Item)) : nullptr;
	return PS && I ? PS->Inventory->CountOf(I) : 0;
}

bool UAerondelFeature::TakeItem(const FString& Item, int32 N) const
{
	AAerondelPlayerState* PS = GetPS(); const UGameDatabase* Db = UGameDatabase::Get();
	UItemDef* I = Db ? Db->FindItem(Id(Item)) : nullptr;
	if (!PS || !I || N <= 0 || PS->Inventory->CountOf(I) < N) return N <= 0;
	PS->Inventory->Remove(I, N);
	return true;
}

void UAerondelFeature::GiveItem(const FString& Item, int32 N) const
{
	AAerondelPlayerState* PS = GetPS(); const UGameDatabase* Db = UGameDatabase::Get();
	UItemDef* I = Db ? Db->FindItem(Id(Item)) : nullptr;
	if (!PS || !I || N <= 0) return;
	const int32 Added = PS->Inventory->Add(I, N, true);
	if (Added < N && PS->Bank) PS->Bank->Add(I, N - Added, false);
}

void UAerondelFeature::GiveToBank(const FString& Item, int32 N) const
{
	AAerondelPlayerState* PS = GetPS(); const UGameDatabase* Db = UGameDatabase::Get();
	UItemDef* I = Db ? Db->FindItem(Id(Item)) : nullptr;
	if (PS && PS->Bank && I && N > 0) PS->Bank->Add(I, N, false);
}

int64 UAerondelFeature::JInt(const FJsonObject& O, const TCHAR* Key, int64 Def) { double V = (double)Def; O.TryGetNumberField(Key, V); return (int64)FMath::FloorToDouble(V); }
double UAerondelFeature::JNum(const FJsonObject& O, const TCHAR* Key, double Def) { double V = Def; O.TryGetNumberField(Key, V); return V; }
bool UAerondelFeature::JBool(const FJsonObject& O, const TCHAR* Key) { bool V = false; O.TryGetBoolField(Key, V); return V; }
FString UAerondelFeature::JStr(const FJsonObject& O, const TCHAR* Key) { FString V; O.TryGetStringField(Key, V); return V; }
TSharedPtr<FJsonObject> UAerondelFeature::JObj(const FJsonObject& O, const TCHAR* Key) { const TSharedPtr<FJsonObject>* V = nullptr; return O.TryGetObjectField(Key, V) && V ? *V : nullptr; }

TArray<FString> UAerondelFeature::JStrArr(const FJsonObject& O, const TCHAR* Key)
{
	TArray<FString> Out; const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (O.TryGetArrayField(Key, A) && A) for (const TSharedPtr<FJsonValue>& V : *A) if (V.IsValid() && V->Type == EJson::String) Out.Add(V->AsString());
	return Out;
}

void UAerondelFeature::PutStrArr(FJsonObject& O, const TCHAR* Key, const TArray<FString>& A)
{
	TArray<TSharedPtr<FJsonValue>> Out; for (const FString& S : A) Out.Add(MakeShared<FJsonValueString>(S));
	O.SetArrayField(Key, Out);
}

void UAerondelFeature::PutIntMap(FJsonObject& O, const TCHAR* Key, const TMap<FString, int32>& M)
{
	TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>(); for (const TPair<FString, int32>& P : M) J->SetNumberField(P.Key, P.Value);
	O.SetObjectField(Key, J);
}

TMap<FString, int32> UAerondelFeature::JIntMap(const FJsonObject& O, const TCHAR* Key)
{
	TMap<FString, int32> Out;
	if (TSharedPtr<FJsonObject> J = JObj(O, Key))
		for (const auto& P : J->Values) if (P.Value.IsValid() && P.Value->Type == EJson::Number) Out.Add(FString(*P.Key), (int32)P.Value->AsNumber());
	return Out;
}
