#include "Models/AerondelModelLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "World/AerondelScalability.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace AerondelRigs
{
	struct FRig { FString Mesh; TMap<FName, FString> Anims; };
	static bool bLoaded = false;
	static TMap<FString, FRig> Rigs;
	static TMap<FString, FString> Monsters, Npcs;
	static FString Player;
	static const FName StateTag(TEXT("AerondelAnim:"));

	static void Load()
	{
		if (bLoaded) return;
		bLoaded = true; Rigs.Reset(); Monsters.Reset(); Npcs.Reset(); Player.Reset();
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), TEXT("rigs.json")))) return;
		TSharedPtr<FJsonObject> Root; const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(R, Root) || !Root.IsValid()) { UE_LOG(LogTemp, Warning, TEXT("[Aerondel] rigs.json did not parse")); return; }
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (Root->TryGetObjectField(TEXT("rigs"), O))
			for (const auto& P : (*O)->Values)
			{
				const TSharedPtr<FJsonObject>* RO = nullptr; if (!P.Value.IsValid() || !P.Value->TryGetObject(RO)) continue;
				FRig Rig; (*RO)->TryGetStringField(TEXT("mesh"), Rig.Mesh);
				const TSharedPtr<FJsonObject>* A = nullptr;
				if ((*RO)->TryGetObjectField(TEXT("anims"), A)) for (const auto& Q : (*A)->Values) { FString S; if (Q.Value.IsValid() && Q.Value->TryGetString(S)) Rig.Anims.Add(FName(*Q.Key), S); }
				if (!Rig.Mesh.IsEmpty()) Rigs.Add(P.Key, Rig);
			}
		auto Map = [&Root](const TCHAR* Field, TMap<FString, FString>& Out)
		{
			const TSharedPtr<FJsonObject>* M = nullptr;
			if (Root->TryGetObjectField(Field, M)) for (const auto& P : (*M)->Values) { FString S; if (P.Value.IsValid() && P.Value->TryGetString(S)) Out.Add(P.Key.ToLower(), S); }
		};
		Map(TEXT("monsters"), Monsters); Map(TEXT("npcs"), Npcs);
		Root->TryGetStringField(TEXT("player"), Player);
		UE_LOG(LogTemp, Log, TEXT("[Aerondel] rigs.json: %d rigs, %d monster mappings, %d npc mappings"), Rigs.Num(), Monsters.Num(), Npcs.Num());
	}
}

void UAerondelModelLibrary::ReloadRigTable() { AerondelRigs::bLoaded = false; AerondelRigs::Load(); }

FString UAerondelModelLibrary::ResolveKey(const FString& Key)
{
	AerondelRigs::Load();
	if (Key == TEXT("player")) return AerondelRigs::Player;
	if (Key.StartsWith(TEXT("mob:"))) return AerondelRigs::Monsters.FindRef(Key.Mid(4).ToLower());
	if (Key.StartsWith(TEXT("npc:"))) return AerondelRigs::Npcs.FindRef(Key.Mid(4).ToLower());
	return Key;
}

bool UAerondelModelLibrary::ApplyRig(USkeletalMeshComponent* Comp, const FString& Key)
{
	if (!Comp) return false;
	const FString K = ResolveKey(Key);
	const AerondelRigs::FRig* Rig = AerondelRigs::Rigs.Find(K);
	if (!Rig) return false;
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Rig->Mesh);
	if (!Mesh) return false;
	Comp->SetSkeletalMeshAsset(Mesh);
	Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Comp->ComponentTags.RemoveAll([](const FName& T) { return T.ToString().StartsWith(TEXT("AerondelRig:")) || T.ToString().StartsWith(TEXT("AerondelAnim:")); });
	Comp->ComponentTags.Add(FName(*(TEXT("AerondelRig:") + K)));
	const EAerondelTier Tier = UAerondelScalability::GetSavedTier();
	const bool bLow = Tier == EAerondelTier::Low || Tier == EAerondelTier::Quest;
	Comp->VisibilityBasedAnimTickOption = bLow ? EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered : EVisibilityBasedAnimTickOption::AlwaysTickPose;
	Comp->SetCastShadow(!bLow);
	SetAnimState(Comp, TEXT("idle"));
	return true;
}

void UAerondelModelLibrary::SetAnimState(USkeletalMeshComponent* Comp, FName State)
{
	if (!Comp) return;
	FString K; FName Cur;
	for (const FName& T : Comp->ComponentTags)
	{
		const FString S = T.ToString();
		if (S.StartsWith(TEXT("AerondelRig:"))) K = S.Mid(12);
		else if (S.StartsWith(TEXT("AerondelAnim:"))) Cur = FName(*S.Mid(13));
	}
	if (K.IsEmpty() || Cur == State) return;
	const AerondelRigs::FRig* Rig = AerondelRigs::Rigs.Find(K); if (!Rig) return;
	const FString* Path = Rig->Anims.Find(State);
	if (!Path && State == FName(TEXT("run"))) Path = Rig->Anims.Find(TEXT("walk"));
	if (!Path) Path = Rig->Anims.Find(TEXT("idle"));
	if (!Path) return;
	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, **Path);
	if (!Seq) return;
	Comp->ComponentTags.RemoveAll([](const FName& T) { return T.ToString().StartsWith(TEXT("AerondelAnim:")); });
	Comp->ComponentTags.Add(FName(*(TEXT("AerondelAnim:") + State.ToString())));
	const bool bLoop = State != FName(TEXT("death")) && State != FName(TEXT("attack")) && State != FName(TEXT("hit"));
	Comp->PlayAnimation(Seq, bLoop);
}
