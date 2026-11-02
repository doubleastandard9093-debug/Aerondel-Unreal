#include "Features/RegionCampsFeature.h"
#include "Combat/AerondelMob.h"
#include "Data/GameDatabase.h"
#include "Data/ContentDefs.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void URegionCampsFeature::LoadCamps()
{
	bLoaded = true;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), TEXT("world.json")))) return;
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return;
	const TArray<TSharedPtr<FJsonValue>>* Regions = nullptr;
	if (!Root->TryGetArrayField(TEXT("regions"), Regions) || !Regions) return;
	for (const TSharedPtr<FJsonValue>& RV : *Regions)
	{
		const TSharedPtr<FJsonObject> R = RV.IsValid() ? RV->AsObject() : nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Mobs = nullptr;
		if (!R.IsValid() || !R->TryGetArrayField(TEXT("mobs"), Mobs) || !Mobs || Mobs->Num() == 0) continue;
		FCamp C;
		C.Name = JStr(*R, TEXT("name"));
		C.Center = FVector2D(JNum(*R, TEXT("x")), JNum(*R, TEXT("z")));
		C.Radius = (float)JNum(*R, TEXT("r"), 50.0);
		for (const TSharedPtr<FJsonValue>& MV : *Mobs)
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (!MV.IsValid() || !MV->TryGetArray(A) || !A || A->Num() < 3) continue;
			FCampMob M;
			M.Type = (*A)[0]->AsString();
			M.Pos = C.Center + FVector2D((*A)[1]->AsNumber(), (*A)[2]->AsNumber());
			M.Roam = A->Num() > 3 ? (float)(*A)[3]->AsNumber() : 2.f;
			C.Mobs.Add(M);
		}
		Camps.Add(MoveTemp(C));
	}
	UE_LOG(LogTemp, Log, TEXT("[Aerondel] RegionCamps: %d camps loaded"), Camps.Num());
}

void URegionCampsFeature::Raise(FCamp& C)
{
	C.bUp = true;
	UGameInstance* GI = GetGameInstance();
	UWorld* W = GI ? GI->GetWorld() : nullptr;
	const UGameDatabase* Db = UGameDatabase::Get();
	if (!W || !Db) return;
	for (const FCampMob& M : C.Mobs)
	{
		UMobDef* Def = Db->FindMob(Id(M.Type));
		if (!Def) { UE_LOG(LogTemp, Warning, TEXT("[Aerondel] RegionCamps: no mob '%s' (%s)"), *M.Type, *C.Name); continue; }
		FVector At = WebToWorld(M.Pos.X, M.Pos.Y);
		FHitResult Hit;
		if (W->LineTraceSingleByChannel(Hit, WebToWorld(M.Pos.X, M.Pos.Y, 200.0), WebToWorld(M.Pos.X, M.Pos.Y, -200.0), ECC_Visibility)) At = Hit.ImpactPoint;
		At.Z += Def->BodySize.Z * 0.5f + 10.f;
		const FTransform Xf(FRotator::ZeroRotator, At);
		AAerondelMob* A = W->SpawnActorDeferred<AAerondelMob>(AAerondelMob::StaticClass(), Xf, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!A) continue;
		A->Definition = Def;
		A->WanderRadius = FMath::Max(100.f, M.Roam * 100.f); // web metres -> cm
		A->FinishSpawning(Xf);
		C.Live.Add(A);
	}
}

void URegionCampsFeature::Drop(FCamp& C)
{
	for (const TWeakObjectPtr<AAerondelMob>& M : C.Live) if (M.IsValid()) M->Destroy();
	C.Live.Reset();
	C.bUp = false;
}

void URegionCampsFeature::OnGameTick()
{
	if (!bLoaded) LoadCamps();
	FVector2D P;
	if (!PlayerWebPos(P)) return;
	for (FCamp& C : Camps)
	{
		const float D = FVector2D::Distance(P, C.Center);
		if (!C.bUp && D < C.Radius + 40.f) Raise(C);
		else if (C.bUp && D > C.Radius + 60.f) Drop(C);
	}
}

void URegionCampsFeature::Deinitialize()
{
	for (FCamp& C : Camps) Drop(C);
	Super::Deinitialize();
}

int32 URegionCampsFeature::ActiveCampMobs() const
{
	int32 N = 0;
	for (const FCamp& C : Camps) for (const TWeakObjectPtr<AAerondelMob>& M : C.Live) if (M.IsValid()) ++N;
	return N;
}
