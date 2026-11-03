#include "Models/AerondelModelLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
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
	static float Yaw = -90.f;
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
				if ((*RO)->TryGetObjectField(TEXT("anims"), A)) for (const auto& Q : (*A)->Values) { FString S; if (Q.Value.IsValid() && Q.Value->TryGetString(S)) Rig.Anims.Add(FName(*FString(*Q.Key)), S); }
				if (!Rig.Mesh.IsEmpty()) Rigs.Add(FString(*P.Key), Rig);
			}
		auto Map = [&Root](const TCHAR* Field, TMap<FString, FString>& Out)
		{
			const TSharedPtr<FJsonObject>* M = nullptr;
			if (Root->TryGetObjectField(Field, M)) for (const auto& P : (*M)->Values) { FString S; if (P.Value.IsValid() && P.Value->TryGetString(S)) Out.Add(FString(*P.Key).ToLower(), S); }
		};
		Map(TEXT("monsters"), Monsters); Map(TEXT("npcs"), Npcs);
		Root->TryGetStringField(TEXT("player"), Player);
		double Y = -90.0; if (Root->TryGetNumberField(TEXT("yaw"), Y)) Yaw = (float)Y;
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
	if (!bLow) ApplyRealism(Comp, K);
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
	if (K.IsEmpty()) return;
	static const FName Death(TEXT("death")), Attack(TEXT("attack")), Hit(TEXT("hit")), Idle(TEXT("idle"));
	if (Cur == Death && State != Idle) return; // stays down until respawn (ApplyRig / idle)
	const bool bOneShot = State == Attack || State == Hit || State == Death || State == FName(TEXT("gather"));
	if (Cur == State && (!bOneShot || State == Death || Comp->IsPlaying())) return; // one-shots may replay once finished
	if (!bOneShot && (Cur == Attack || Cur == Hit || Cur == FName(TEXT("gather"))) && Comp->IsPlaying()) return; // let the swing finish
	const AerondelRigs::FRig* Rig = AerondelRigs::Rigs.Find(K); if (!Rig) return;
	const FString* Path = Rig->Anims.Find(State);
	if (!Path && State == FName(TEXT("run"))) Path = Rig->Anims.Find(TEXT("walk"));
	if (!Path) Path = Rig->Anims.Find(TEXT("idle"));
	if (!Path) return;
	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, **Path);
	if (!Seq) return;
	Comp->ComponentTags.RemoveAll([](const FName& T) { return T.ToString().StartsWith(TEXT("AerondelAnim:")); });
	Comp->ComponentTags.Add(FName(*(TEXT("AerondelAnim:") + State.ToString())));
	const bool bLoop = !bOneShot;
	Comp->PlayAnimation(Seq, bLoop);
}

void UAerondelModelLibrary::FitRig(USkeletalMeshComponent* Comp, float HeightCm, float HalfHeight)
{
	USkeletalMesh* Mesh = Comp ? Comp->GetSkeletalMeshAsset() : nullptr; if (!Mesh) return;
	const FBox B = Mesh->GetBounds().GetBox();
	const FVector S = B.GetSize();
	if (S.Z < 1.0) return;
	// Quadrupeds (longer than tall) fit their length to 1/0.6 of the target so a wolf is not a pony.
	const double Len = FMath::Max(S.X, S.Y);
	const double K = Len > S.Z * 1.3 ? (HeightCm / 0.6) / Len : HeightCm / S.Z;
	Comp->SetRelativeScale3D(FVector(K));
	Comp->SetRelativeLocation(FVector(0, 0, -HalfHeight - B.Min.Z * K));
	Comp->SetRelativeRotation(FRotator(0, AerondelRigs::Yaw, 0));
}

bool UAerondelModelLibrary::IsBusy(USkeletalMeshComponent* Comp)
{
	if (!Comp || !Comp->IsPlaying()) return false;
	for (const FName& T : Comp->ComponentTags) if (T == FName(TEXT("AerondelAnim:attack")) || T == FName(TEXT("AerondelAnim:hit")) || T == FName(TEXT("AerondelAnim:gather"))) return true;
	return false;
}

namespace AerondelRealism
{
	/** Web js/realism.js surfaceKind(), ported. */
	static FString Kind(const FString& MatName, const FString& Model)
	{
		const FString N = MatName.ToLower(), M = Model.ToLower();
		auto Any = [&N](std::initializer_list<const TCHAR*> L) { for (const TCHAR* W : L) if (N.Contains(W)) return true; return false; };
		auto ModelAny = [&M](std::initializer_list<const TCHAR*> L) { for (const TCHAR* W : L) if (M.Contains(W)) return true; return false; };
		if (Any({TEXT("eye")})) return TEXT("horn");  // wet glossy
		if (Any({TEXT("gold"), TEXT("metal"), TEXT("steel"), TEXT("iron"), TEXT("grey"), TEXT("gray"), TEXT("silver")})) return TEXT("metal");
		if (Any({TEXT("horn"), TEXT("hoove"), TEXT("teeth"), TEXT("tooth"), TEXT("nose"), TEXT("claw"), TEXT("tongue"), TEXT("mouth")})) return TEXT("horn");
		if (Any({TEXT("belt"), TEXT("strap"), TEXT("brown"), TEXT("leather"), TEXT("boot")})) return TEXT("leather");
		if (Any({TEXT("wood")})) return TEXT("wood");
		if (Any({TEXT("hair"), TEXT("eyebrow")})) return TEXT("hair");
		if (Any({TEXT("skin"), TEXT("muzzle")})) return TEXT("skin");
		if (Any({TEXT("main"), TEXT("secondary"), TEXT("light")}) || N.StartsWith(TEXT("material")))
			return ModelAny({TEXT("wolf"), TEXT("cow"), TEXT("yeti"), TEXT("spider")}) ? TEXT("fur")
				: ModelAny({TEXT("dragon"), TEXT("demon"), TEXT("orc"), TEXT("goblin"), TEXT("giant"), TEXT("zombie")}) ? TEXT("hide") : TEXT("cloth");
		if (Any({TEXT("atlas")})) return M.Contains(TEXT("skeleton")) ? TEXT("bone") : TEXT("hide");
		return TEXT("cloth");
	}
}

void UAerondelModelLibrary::ApplyRealism(USkeletalMeshComponent* Comp, const FString& RigKey)
{
	if (!Comp) return;
	const EAerondelTier Tier = UAerondelScalability::GetSavedTier();
	if (Tier == EAerondelTier::Low || Tier == EAerondelTier::Quest) return;
	const TArray<FName> Slots = Comp->GetMaterialSlotNames();
	for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
	{
		UMaterialInterface* Orig = Comp->GetMaterial(i);
		if (!Orig || Orig->IsA<UMaterialInstanceDynamic>()) continue;
		const FString SlotName = Slots.IsValidIndex(i) ? Slots[i].ToString() : Orig->GetName();
		const FString Kind = AerondelRealism::Kind(SlotName + TEXT(" ") + Orig->GetName(), RigKey);
		const FString Path = FString::Printf(TEXT("/Game/Gielinor/Materials/Real/MI_Real_%s.MI_Real_%s"), *Kind, *Kind);
		UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, *Path);
		if (!Master) return;  // realism materials not built yet
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Master, Comp);
		// Interchange glTF material instances: BaseColorTexture / BaseColorFactor (older importers: BaseColor / Color).
		UTexture* Tex = nullptr;
		for (const TCHAR* P : {TEXT("BaseColorTexture"), TEXT("BaseColorMap"), TEXT("BaseColor")})
			if (Orig->GetTextureParameterValue(FHashedMaterialParameterInfo(FName(P)), Tex) && Tex) break;
		if (Tex) { D->SetTextureParameterValue(TEXT("BaseTex"), Tex); D->SetScalarParameterValue(TEXT("UseTex"), 1.f); }
		FLinearColor C = FLinearColor::White;
		for (const TCHAR* P : {TEXT("BaseColorFactor"), TEXT("BaseColor"), TEXT("Color")})
			if (Orig->GetVectorParameterValue(FHashedMaterialParameterInfo(FName(P)), C)) break;
		D->SetVectorParameterValue(TEXT("Tint"), C);
		Comp->SetMaterial(i, D);
	}
}

bool UAerondelModelLibrary::MobTint(FName MobId, FLinearColor& OutColor, float& OutK)
{
	// Generated from Gielinor-Lite js/rigs.js MONSTER_RIGS (first variant of each mob that carries a tint).
	struct FT { const TCHAR* Id; uint32 Hex; float K; };
	static const FT All[] = {
		{ TEXT("ogre"), 0x9a8a5au, 0.55f }, { TEXT("grumsh"), 0xb05a3au, 0.55f }, { TEXT("bog_giant"), 0x5a8a4au, 0.55f },
		{ TEXT("crag_giant"), 0x9a9a9au, 0.55f }, { TEXT("ice_giant"), 0x9cc8ecu, 0.55f }, { TEXT("green_dragon"), 0x5ab05au, 0.55f },
		{ TEXT("scorchwing"), 0xd05a2au, 0.55f }, { TEXT("bandit"), 0x8a6a4au, 0.3f }, { TEXT("guard"), 0xb0b0c0u, 0.35f },
		{ TEXT("ironveil_knight"), 0x9aa0b8u, 0.5f }, { TEXT("dark_archer"), 0x4a4a5au, 0.5f }, { TEXT("ashen_raider"), 0x8a7a70u, 0.4f },
		{ TEXT("barrow_ghoul"), 0x9ab08au, 0.4f }, { TEXT("cinder_warden"), 0xc0502au, 0.4f }, { TEXT("thornback_prowler"), 0x5a7a3au, 0.5f },
		{ TEXT("bramble_colossus"), 0x5a7a3au, 0.55f }, { TEXT("hoarfrost_stalker"), 0xe0ecf8u, 0.5f }, { TEXT("glacier_behemoth"), 0xc0dcf4u, 0.6f },
		{ TEXT("storm_gryphon"), 0x9a8a6au, 0.55f }, { TEXT("peak_warlord"), 0x6a6a7au, 0.5f }, { TEXT("murk_leech"), 0x2a3a2au, 0.55f },
		{ TEXT("bog_revenant"), 0x7a9a7au, 0.5f }, { TEXT("glass_scorpion"), 0xd0d8e0u, 0.55f }, { TEXT("dune_tyrant"), 0xc89a6au, 0.55f },
		{ TEXT("caldera_drake"), 0xa0301au, 0.55f }, { TEXT("molten_archon"), 0xff6a1au, 0.5f }, { TEXT("voidborn_knight"), 0x4a3a7au, 0.6f },
		{ TEXT("sunderlord_vexmar"), 0x6a4aa0u, 0.6f }, { TEXT("drowned_deckhand"), 0x6ab0a0u, 0.4f }, { TEXT("drowned_gunner"), 0x5a9ab0u, 0.4f },
		{ TEXT("captain_saltgrave"), 0x4a8a9au, 0.5f } };
	for (const FT& T : All)
		if (MobId == FName(T.Id))
		{
			OutColor = FLinearColor(FColor((T.Hex >> 16) & 0xFF, (T.Hex >> 8) & 0xFF, T.Hex & 0xFF));
			OutK = T.K;
			return true;
		}
	return false;
}

void UAerondelModelLibrary::ApplyTint(USkeletalMeshComponent* Comp, const FLinearColor& Color, float K, TArray<FLinearColor>* Cache)
{
	if (!Comp) return;
	const float A = FMath::Clamp(K, 0.f, 1.f);
	const bool bFill = Cache && Cache->Num() != Comp->GetNumMaterials();
	if (bFill) Cache->Reset();
	for (int32 i = 0; i < Comp->GetNumMaterials(); ++i)
	{
		UMaterialInterface* Mat = Comp->GetMaterial(i);
		if (!Mat) { if (bFill) Cache->Add(FLinearColor::White); continue; }
		UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(Mat);
		if (!D) { D = UMaterialInstanceDynamic::Create(Mat, Comp); Comp->SetMaterial(i, D); }
		FLinearColor Base = FLinearColor::White;
		for (const TCHAR* P : { TEXT("Tint"), TEXT("BaseColorFactor"), TEXT("BaseColor"), TEXT("Color") })
			if (D->GetVectorParameterValue(FHashedMaterialParameterInfo(FName(P)), Base)) break;
		if (bFill) Cache->Add(Base);
		else if (Cache && Cache->IsValidIndex(i)) Base = (*Cache)[i];
		const FLinearColor Out = FMath::Lerp(Base, Color, A);
		for (const TCHAR* P : { TEXT("Tint"), TEXT("BaseColorFactor"), TEXT("BaseColor"), TEXT("Color") })
			D->SetVectorParameterValue(FName(P), Out);
	}
}
