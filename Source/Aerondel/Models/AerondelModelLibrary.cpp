#include "Models/AerondelModelLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "ReferenceSkeleton.h"
#include "World/AerondelScalability.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Char.h"

static TAutoConsoleVariable<int32> CVarAerondelBindPose(TEXT("aerondel.Rig.BindPose"), 0,
	TEXT("Pose diagnostic: 0 = animate normally, 1 = hold the player rig in its bind (reference) pose, 2 = all rigs in bind pose."));

namespace AerondelRigs
{
	struct FRig { FString Mesh; TMap<FName, FString> Anims; TMap<FName, float> AnimRates; TOptional<float> Yaw, Pitch, Roll; };
	static bool bLoaded = false;
	static TMap<FString, FRig> Rigs;
	static TMap<FString, FString> Monsters, Npcs;
	static FString Player;
	static float Yaw = 0.f, Pitch = 0.f, Roll = 0.f;
	static const FName StateTag(TEXT("AerondelAnim:"));
	static TSet<FString> MissingWarned, AnimWarned;

	static FRotator RotationFor(const FRig* Rig)
	{
		return FRotator(
			Rig && Rig->Pitch.IsSet() ? Rig->Pitch.GetValue() : Pitch,
			Rig && Rig->Yaw.IsSet() ? Rig->Yaw.GetValue() : Yaw,
			Rig && Rig->Roll.IsSet() ? Rig->Roll.GetValue() : Roll);
	}

	static const FRig* RigOn(const USkeletalMeshComponent* Comp)
	{
		if (!Comp) return nullptr;
		for (const FName& T : Comp->ComponentTags)
		{
			const FString S = T.ToString();
			if (S.StartsWith(TEXT("AerondelRig:"))) return Rigs.Find(S.Mid(12));
		}
		return nullptr;
	}

	static void StripTags(USkeletalMeshComponent* Comp, bool bRig, bool bAnim)
	{
		Comp->ComponentTags.RemoveAll([bRig, bAnim](const FName& T)
		{
			const FString S = T.ToString();
			return (bRig && S.StartsWith(TEXT("AerondelRig:"))) || (bAnim && S.StartsWith(TEXT("AerondelAnim:")));
		});
	}

	static void HoldRefPose(USkeletalMeshComponent* Comp, const TCHAR* Why)
	{
		if (!Comp) return;
		Comp->Stop();
		Comp->SetAnimation(nullptr);
		StripTags(Comp, false, true);
		Comp->ComponentTags.Add(FName(TEXT("AerondelAnim:bindpose")));
		Comp->RefreshBoneTransforms();
		if (!Why) return;
		FString Id = Comp->GetName();
		for (const FName& T : Comp->ComponentTags)
		{
			const FString S = T.ToString();
			if (S.StartsWith(TEXT("AerondelRig:"))) { Id = S.Mid(12); break; }
		}
		const FString Key = Id + TEXT(":") + Why;
		if (!AnimWarned.Contains(Key))
		{
			AnimWarned.Add(Key);
			UE_LOG(LogTemp, Warning, TEXT("[Aerondel] %s: holding reference pose (%s)"), *Id, Why);
		}
	}

	static bool AnimUsable(const UAnimSequence* Seq, const USkeletalMesh* Mesh, FString& Why)
	{
		if (!Seq || !Mesh) { Why = TEXT("missing asset"); return false; }
		if (Seq->GetAdditiveAnimType() != AAT_None) { Why = TEXT("additive clip with no base pose"); return false; }
		const USkeleton* AnimSkel = Seq->GetSkeleton();
		const USkeleton* MeshSkel = Mesh->GetSkeleton();
		if (!AnimSkel || !MeshSkel) { Why = TEXT("sequence or mesh has no skeleton"); return false; }
		if (AnimSkel != MeshSkel)
		{
			Why = FString::Printf(TEXT("anim skeleton '%s' != mesh skeleton '%s'"), *AnimSkel->GetName(), *MeshSkel->GetName());
			return false;
		}
		return true;
	}


	static uint32 StableIdHash(const FString& S)
	{
		uint32 H = 2166136261u;
		for (const TCHAR C : S)
		{
			H ^= (uint32)FChar::ToLower(C);
			H *= 16777619u;
		}
		return H;
	}

	/** Creature rig to use when an unmapped id obviously names that creature. Feature NPCs such as skywyrms stay folk. */
	static const TCHAR* ObviousBeastRig(const FString& Id)
	{
		static const TCHAR* Beasts[] = {
			TEXT("spider"), TEXT("wolf"), TEXT("dragon"), TEXT("cow"), TEXT("goblin"), TEXT("orc"),
			TEXT("demon"), TEXT("skeleton"), TEXT("zombie"), TEXT("yeti"), TEXT("giant"), TEXT("ghost"),
		};
		for (const TCHAR* B : Beasts)
		{
			const FString Name(B);
			if (Id.Equals(Name, ESearchCase::IgnoreCase)
				|| Id.StartsWith(Name + TEXT("_"), ESearchCase::IgnoreCase)
				|| Id.EndsWith(TEXT("_") + Name, ESearchCase::IgnoreCase))
			{
				if (Rigs.Contains(Name)) return B;
			}
		}
		return nullptr;
	}

	static FString DefaultCharacterRig(const FString& Id)
	{
		if (const TCHAR* Beast = ObviousBeastRig(Id)) return FString(Beast);
		TArray<FString> Folks;
		for (const TPair<FString, FRig>& P : Rigs)
		{
			if (P.Key.StartsWith(TEXT("folk_"))) Folks.Add(P.Key);
		}
		Folks.Sort();
		if (Folks.Num() > 0) return Folks[StableIdHash(Id) % Folks.Num()];
		if (Rigs.Contains(TEXT("adventurer"))) return TEXT("adventurer");
		return FString();
	}

	static void Load()
	{
		if (bLoaded) return;
		bLoaded = true; Rigs.Reset(); Monsters.Reset(); Npcs.Reset(); Player.Reset();
		MissingWarned.Reset(); AnimWarned.Reset();
		Yaw = 0.f; Pitch = 0.f; Roll = 0.f;
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
				const TSharedPtr<FJsonObject>* Rates = nullptr;
				if ((*RO)->TryGetObjectField(TEXT("animRates"), Rates))
					for (const auto& Q : (*Rates)->Values)
					{
						double V = 1.0;
						if (Q.Value.IsValid() && Q.Value->TryGetNumber(V)) Rig.AnimRates.Add(FName(*FString(*Q.Key)), (float)V);
					}
				double RY = 0, RP = 0, RR = 0;
				if ((*RO)->TryGetNumberField(TEXT("yaw"), RY)) Rig.Yaw = (float)RY;
				if ((*RO)->TryGetNumberField(TEXT("pitch"), RP)) Rig.Pitch = (float)RP;
				if ((*RO)->TryGetNumberField(TEXT("roll"), RR)) Rig.Roll = (float)RR;
				if (!Rig.Mesh.IsEmpty()) Rigs.Add(FString(*P.Key), Rig);
			}
		auto Map = [&Root](const TCHAR* Field, TMap<FString, FString>& Out)
		{
			const TSharedPtr<FJsonObject>* M = nullptr;
			if (Root->TryGetObjectField(Field, M)) for (const auto& P : (*M)->Values) { FString S; if (P.Value.IsValid() && P.Value->TryGetString(S)) Out.Add(FString(*P.Key).ToLower(), S); }
		};
		Map(TEXT("monsters"), Monsters); Map(TEXT("npcs"), Npcs);
		Root->TryGetStringField(TEXT("player"), Player);
		double Y = 0.0, Pch = 0.0, Rl = 0.0;
		if (Root->TryGetNumberField(TEXT("yaw"), Y)) Yaw = (float)Y;
		if (Root->TryGetNumberField(TEXT("pitch"), Pch)) Pitch = (float)Pch;
		if (Root->TryGetNumberField(TEXT("roll"), Rl)) Roll = (float)Rl;
		// Old files defaulted to yaw=-90 for "glTF faces +Y". Auto-align now owns facing; applying
		// that default on top of it laid a standing mesh on its back.
		if (FMath::IsNearlyEqual(Yaw, -90.f) && FMath::IsNearlyEqual(Pitch, 0.f) && FMath::IsNearlyEqual(Roll, 0.f))
		{
			UE_LOG(LogTemp, Log, TEXT("[Aerondel] rigs.json: ignoring legacy yaw -90; bind-pose auto-align sets facing"));
			Yaw = 0.f;
		}
		UE_LOG(LogTemp, Log, TEXT("[Aerondel] rigs.json: %d rigs, %d monster mappings, %d npc mappings, extra rotation P%.1f Y%.1f R%.1f"),
			Rigs.Num(), Monsters.Num(), Npcs.Num(), Pitch, Yaw, Roll);
	}
}

void UAerondelModelLibrary::ReloadRigTable() { AerondelRigs::bLoaded = false; AerondelRigs::Load(); }

FString UAerondelModelLibrary::ResolveKey(const FString& Key)
{
	AerondelRigs::Load();
	if (Key == TEXT("player")) return AerondelRigs::Player;
	if (Key.StartsWith(TEXT("mob:")))
	{
		const FString Id = Key.Mid(4).ToLower();
		if (const FString* Found = AerondelRigs::Monsters.Find(Id)) return *Found;
		return AerondelRigs::DefaultCharacterRig(Id);
	}
	if (Key.StartsWith(TEXT("npc:")))
	{
		const FString Id = Key.Mid(4).ToLower();
		if (const FString* Found = AerondelRigs::Npcs.Find(Id)) return *Found;
		return AerondelRigs::DefaultCharacterRig(Id);
	}
	return Key;
}

int32 UAerondelModelLibrary::GetBindPoseMode()
{
	static bool bArgChecked = false;
	if (!bArgChecked)
	{
		bArgChecked = true;
		if (FParse::Param(FCommandLine::Get(), TEXT("AerondelBindPose")) && CVarAerondelBindPose.GetValueOnGameThread() == 0) CVarAerondelBindPose->Set(1, ECVF_SetByCommandline);
	}
	return CVarAerondelBindPose.GetValueOnGameThread();
}

void UAerondelModelLibrary::SetBindPoseMode(int32 Mode, USkeletalMeshComponent* PlayerMesh)
{
	GetBindPoseMode();
	CVarAerondelBindPose->Set(FMath::Clamp(Mode, 0, 2), ECVF_SetByConsole);
	UE_LOG(LogTemp, Log, TEXT("[Aerondel] RigDiag: bind-pose mode %d (0 = animated, 1 = player, 2 = all)"), FMath::Clamp(Mode, 0, 2));
	if (!PlayerMesh) return;
	PlayerMesh->ComponentTags.RemoveAll([](const FName& T) { return T.ToString().StartsWith(TEXT("AerondelAnim:")); });
	SetAnimState(PlayerMesh, TEXT("idle")); // re-evaluates: holds the bind pose when on, restarts idle when off
}

void UAerondelModelLibrary::ClearRig(USkeletalMeshComponent* Comp)
{
	if (!Comp) return;
	Comp->Stop();
	Comp->SetAnimation(nullptr);
	Comp->SetSkeletalMeshAsset(nullptr);
	AerondelRigs::StripTags(Comp, true, true);
}

bool UAerondelModelLibrary::ApplyRig(USkeletalMeshComponent* Comp, const FString& Key)
{
	if (!Comp) return false;
	const FString K = ResolveKey(Key);
	const AerondelRigs::FRig* Rig = AerondelRigs::Rigs.Find(K);
	if (!Rig)
	{
		ClearRig(Comp);
		if (!AerondelRigs::MissingWarned.Contains(Key))
		{
			AerondelRigs::MissingWarned.Add(Key);
			UE_LOG(LogTemp, Warning, TEXT("[Aerondel] ApplyRig %s: no rig mapped (resolved '%s') — using placeholder (later misses for this id are Verbose)"), *Key, *K);
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("[Aerondel] ApplyRig %s: no rig mapped (resolved '%s')"), *Key, *K);
		}
		return false;
	}
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Rig->Mesh);
	if (!Mesh)
	{
		ClearRig(Comp);
		UE_LOG(LogTemp, Warning, TEXT("[Aerondel] ApplyRig %s: mesh %s did not load"), *Key, *Rig->Mesh);
		return false;
	}
	Comp->SetSkeletalMeshAsset(Mesh);
	Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	AerondelRigs::StripTags(Comp, true, true);
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
	const int32 BindMode = GetBindPoseMode();
	if (BindMode >= 2 || (BindMode == 1 && K == AerondelRigs::Player))
	{
		if (Cur != FName(TEXT("bindpose")))
		{
			Comp->Stop();
			Comp->SetAnimation(nullptr);
			Comp->ComponentTags.RemoveAll([](const FName& T) { return T.ToString().StartsWith(TEXT("AerondelAnim:")); });
			Comp->ComponentTags.Add(FName(TEXT("AerondelAnim:bindpose")));
			Comp->RefreshBoneTransforms();
			UE_LOG(LogTemp, Log, TEXT("[Aerondel] RigDiag %s: held in BIND POSE (animation off, aerondel.Rig.BindPose=%d)"), *K, BindMode);
		}
		return;
	}
	const FName Walk(TEXT("walk")), Run(TEXT("run"));
	const FString* Path = Rig->Anims.Find(State);
	float PlayRate = 1.f;
	if (const float* Rate = Rig->AnimRates.Find(State)) PlayRate = *Rate;
	if (!Path && State == Run) Path = Rig->Anims.Find(Walk);
	if (!Path && State == Idle)
	{
		Path = Rig->Anims.Find(Walk);
		if (!Path) Path = Rig->Anims.Find(Run);
		PlayRate = 0.f; // first frame of walk/run, frozen, instead of the T/reference pose
	}
	if (!Path && State == Attack)
	{
		Path = Rig->Anims.Find(Hit);
		if (!Path) Path = Rig->Anims.Find(Idle);
	}
	if (!Path && State == Hit)
	{
		Path = Rig->Anims.Find(Attack);
		if (!Path) Path = Rig->Anims.Find(Idle);
	}
	if (!Path && State == Death)
	{
		Path = Rig->Anims.Find(Hit);
		if (!Path) Path = Rig->Anims.Find(Attack);
		if (!Path) Path = Rig->Anims.Find(Idle);
	}
	if (!Path) Path = Rig->Anims.Find(Idle);
	if (!Path)
	{
		FString Have;
		for (const TPair<FName, FString>& P : Rig->Anims)
		{
			if (!Have.IsEmpty()) Have += TEXT(",");
			Have += P.Key.ToString();
		}
		AerondelRigs::HoldRefPose(Comp, *FString::Printf(
			TEXT("no clip for this state (rigs.json states: %s)"), Have.IsEmpty() ? TEXT("none") : *Have));
		return;
	}
	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, **Path);
	FString Why;
	if (!AerondelRigs::AnimUsable(Seq, Comp->GetSkeletalMeshAsset(), Why))
	{
		AerondelRigs::HoldRefPose(Comp, *Why);
		return;
	}
	AerondelRigs::StripTags(Comp, false, true);
	Comp->ComponentTags.Add(FName(*(TEXT("AerondelAnim:") + State.ToString())));
	const bool bLoop = !bOneShot;
	Comp->PlayAnimation(Seq, bLoop);
	Comp->SetPlayRate(PlayRate);
	if (PlayRate <= KINDA_SMALL_NUMBER)
	{
		Comp->SetPosition(0.f, false);
	}
}

static TArray<FTransform> ComponentSpaceRefPose(const USkeletalMesh* Mesh)
{
	TArray<FTransform> Cs;
	if (!Mesh) return Cs;
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const int32 N = Ref.GetNum();
	const TArray<FTransform>& Local = Ref.GetRefBonePose();
	Cs.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		const int32 P = Ref.GetParentIndex(i);
		Cs[i] = P >= 0 ? Local[i] * Cs[P] : Local[i];
	}
	return Cs;
}

static bool IsFootBone(const FString& B)
{
	if (B.StartsWith(TEXT("pt.")) || B.StartsWith(TEXT("pt_"))) return false;
	return (B.Contains(TEXT("foot")) || B.Contains(TEXT("toe")) || B.Contains(TEXT("ankle"))) && !B.Contains(TEXT("end"));
}
static bool IsHeadBone(const FString& B)
{
	return B.Contains(TEXT("head")) && !B.Contains(TEXT("end")) && !B.Contains(TEXT("top"));
}
static bool IsLeftBone(const FString& B)
{
	return B.EndsWith(TEXT(".l")) || B.EndsWith(TEXT("_l")) || B.Contains(TEXT(".l.")) || B.Contains(TEXT("_l_")) || B.Contains(TEXT("left"));
}
static bool IsRightBone(const FString& B)
{
	return B.EndsWith(TEXT(".r")) || B.EndsWith(TEXT("_r")) || B.Contains(TEXT(".r.")) || B.Contains(TEXT("_r_")) || B.Contains(TEXT("right"));
}

// When the bind/ref pose is missing or clustered at the origin (Interchange ignored IBMs
// and left every bone at identity), head-feet cannot be measured. Rotate the mesh's
// longest imported bounds axis to +Z so a lying reference pose still stands up.
static FQuat AlignFromMeshBounds(const USkeletalMesh* Mesh)
{
	if (!Mesh) return FQuat::Identity;
	const FBox Box = Mesh->GetBounds().GetBox();
	const FVector Size = Box.GetSize();
	const double X = Size.X, Y = Size.Y, Z = Size.Z;
	const double Longest = FMath::Max3(X, Y, Z);
	if (Longest < 1.0) return FQuat::Identity;
	// Already standing: height is the longest (or nearly) axis.
	if (Z >= Longest * 0.75) return FQuat::Identity;
	const FVector Axis = (X >= Y) ? FVector::ForwardVector : FVector::RightVector;
	return FQuat::FindBetweenNormals(Axis.GetSafeNormal(), FVector::UpVector);
}

// Map the BIND pose so feet->head is +Z and the character faces +X. Using the bind pose (not a
// playing clip) avoids folding a mismatched animation into the estimate. rigs.json pitch/yaw/roll
// are applied on top and default to 0. If the bind pose is missing or degenerate, fall back to
// the imported mesh bounds so a lying reference pose still stands.
static FQuat AlignBindPose(const USkeletalMesh* Mesh, FVector* OutHead, FVector* OutFeet)
{
	if (!Mesh) return FQuat::Identity;
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const TArray<FTransform> Cs = ComponentSpaceRefPose(Mesh);
	const int32 N = Ref.GetNum();
	if (N < 3 || Cs.Num() != N) return AlignFromMeshBounds(Mesh);
	int32 Head = INDEX_NONE;
	FVector Feet = FVector::ZeroVector; int32 NF = 0;
	FVector Left = FVector::ZeroVector, Right = FVector::ZeroVector; int32 NL = 0, NR = 0;
	FVector ShL = FVector::ZeroVector, ShR = FVector::ZeroVector; int32 NSL = 0, NSR = 0;
	FBox BoneBox(ForceInit);
	for (int32 i = 0; i < N; ++i)
	{
		BoneBox += Cs[i].GetLocation();
		const FString B = Ref.GetBoneName(i).ToString().ToLower();
		if (Head == INDEX_NONE && IsHeadBone(B)) Head = i;
		if (IsFootBone(B)) { Feet += Cs[i].GetLocation(); ++NF; }
		const bool bShoulder = B.Contains(TEXT("shoulder")) || B.Contains(TEXT("upperarm")) || (B.Contains(TEXT("arm")) && !B.Contains(TEXT("lower")) && !B.Contains(TEXT("fore")));
		const bool bHip = B.Contains(TEXT("upperleg")) || B.Contains(TEXT("thigh")) || B.Contains(TEXT("upleg"));
		if (bShoulder)
		{
			if (IsLeftBone(B)) { ShL += Cs[i].GetLocation(); ++NSL; }
			else if (IsRightBone(B)) { ShR += Cs[i].GetLocation(); ++NSR; }
		}
		else if (bHip)
		{
			if (IsLeftBone(B)) { Left += Cs[i].GetLocation(); ++NL; }
			else if (IsRightBone(B)) { Right += Cs[i].GetLocation(); ++NR; }
		}
	}
	FVector CP = FVector::ZeroVector; for (int32 i = 0; i < N; ++i) CP += Cs[i].GetLocation(); CP /= N;
	const FVector HeadP = Head != INDEX_NONE ? Cs[Head].GetLocation() : FVector::ZeroVector;
	const FVector FeetP = NF ? Feet / NF : CP;
	if (OutHead) *OutHead = Head != INDEX_NONE ? HeadP : BoneBox.GetCenter();
	if (OutFeet) *OutFeet = NF ? FeetP : BoneBox.Min;
	const FVector Up = Head != INDEX_NONE ? (HeadP - FeetP).GetSafeNormal() : FVector::ZeroVector;
	const bool bDegenerate = Head == INDEX_NONE || Up.IsNearlyZero() || BoneBox.GetExtent().GetMax() < 1.0;
	if (bDegenerate)
	{
		const FVector Size = Mesh->GetBounds().GetBox().GetSize();
		UE_LOG(LogTemp, Warning, TEXT("[Aerondel] AlignBindPose %s: %s (bone box %.2f), using mesh bounds (%.2f x %.2f x %.2f) to stand a lying/missing bind pose"),
			*Mesh->GetName(),
			Head == INDEX_NONE ? TEXT("no head bone") : TEXT("degenerate head-feet / bind pose missing"),
			BoneBox.GetSize().GetMax(), Size.X, Size.Y, Size.Z);
		return AlignFromMeshBounds(Mesh);
	}
	FQuat Q = FQuat::FindBetweenNormals(Up, FVector::UpVector);
	FVector L, R; bool bLR = false;
	if (NSL && NSR) { L = ShL / NSL; R = ShR / NSR; bLR = true; }
	else if (NL && NR) { L = Left / NL; R = Right / NR; bLR = true; }
	if (bLR)
	{
		const FVector Across = (R - L).GetSafeNormal(); // character's right
		const FVector Fwd = FVector::CrossProduct(Across, Up).GetSafeNormal();
		if (!Fwd.IsNearlyZero())
		{
			FVector Fwd1 = Q.RotateVector(Fwd);
			Fwd1.Z = 0.f;
			if (!Fwd1.Normalize()) {}
			else Q = FQuat::FindBetweenNormals(Fwd1, FVector::ForwardVector) * Q;
		}
	}
	return Q;
}

static double LowestFootZ(const TArray<FTransform>& Cs, const FReferenceSkeleton& Ref, const FQuat& Rot, double Scale)
{
	double Low = TNumericLimits<double>::Max();
	int32 NFoot = 0;
	for (int32 i = 0; i < Ref.GetNum() && i < Cs.Num(); ++i)
	{
		if (!IsFootBone(Ref.GetBoneName(i).ToString().ToLower())) continue;
		Low = FMath::Min(Low, Rot.RotateVector(Cs[i].GetLocation() * Scale).Z);
		++NFoot;
	}
	if (NFoot) return Low;
	for (const FTransform& T : Cs) Low = FMath::Min(Low, Rot.RotateVector(T.GetLocation() * Scale).Z);
	return Low;
}

static FVector PosedUp(const USkeletalMesh* Mesh, const USkeletalMeshComponent* Comp)
{
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const TArray<FTransform>& Cs = Comp->GetComponentSpaceTransforms();
	const int32 N = Ref.GetNum();
	if (Cs.Num() != N) return FVector::ZeroVector;
	int32 Head = INDEX_NONE; FVector Feet = FVector::ZeroVector; int32 NF = 0;
	for (int32 i = 0; i < N; ++i)
	{
		const FString B = Ref.GetBoneName(i).ToString().ToLower();
		if (Head == INDEX_NONE && IsHeadBone(B)) Head = i;
		if (IsFootBone(B)) { Feet += Cs[i].GetLocation(); ++NF; }
	}
	if (Head == INDEX_NONE || !NF) return FVector::ZeroVector;
	return (Cs[Head].GetLocation() - Feet / NF).GetSafeNormal();
}

static bool IsHumanoidMesh(const USkeletalMesh* Mesh)
{
	if (!Mesh) return false;
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	bool bHands = false, bHead = false;
	for (int32 i = 0; i < Ref.GetNum(); ++i)
	{
		const FString B = Ref.GetBoneName(i).ToString().ToLower();
		if (B.Contains(TEXT("hand")) || B.Contains(TEXT("arm"))) bHands = true;
		if (B.Contains(TEXT("head")) && !B.Contains(TEXT("end")) && !B.Contains(TEXT("top"))) bHead = true;
	}
	return bHands && bHead;
}

bool UAerondelModelLibrary::FitRig(USkeletalMeshComponent* Comp, float HeightCm, float HalfHeight)
{
	USkeletalMesh* Mesh = Comp ? Comp->GetSkeletalMeshAsset() : nullptr;
	if (!Mesh) { UE_LOG(LogTemp, Warning, TEXT("[Aerondel] FitRig: no skeletal mesh on %s"), Comp ? *Comp->GetPathName() : TEXT("null")); return false; }
	const FBox B = Mesh->GetBounds().GetBox();
	const FVector S = B.GetSize();
	const double RefExtent = BoneExtent(Mesh, nullptr);
	// A lying-down or metre-scale import can have S.Z < 1 cm (the old early-out), so use the longest
	// bound / bone extent. Humanoids are never treated as quadrupeds just because they are on their back.
	double HeightAxis = FMath::Max3(S.X, S.Y, S.Z);
	if (S.Z >= HeightAxis * 0.75) HeightAxis = S.Z;
	if (HeightAxis < 1.0 && RefExtent > 1.0) HeightAxis = RefExtent;
	if (HeightAxis < 1.0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Aerondel] FitRig %s: unusable bounds (%.2f x %.2f x %.2f, bone extent %.2f)"), *Mesh->GetName(), S.X, S.Y, S.Z, RefExtent);
		return false;
	}
	const double Len = FMath::Max(S.X, S.Y);
	const bool bQuad = !IsHumanoidMesh(Mesh) && S.Z >= 1.0 && Len > S.Z * 1.3;
	const double K = bQuad ? (HeightCm / 0.6) / Len : HeightCm / HeightAxis;
	const FQuat Align = AlignBindPose(Mesh, nullptr, nullptr);
	const FQuat Rot = AerondelRigs::RotationFor(AerondelRigs::RigOn(Comp)).Quaternion() * Align;
	Comp->SetRelativeScale3D(FVector(K));
	Comp->SetRelativeRotation(Rot);
	{
		const TArray<FTransform> BindCs = ComponentSpaceRefPose(Mesh);
		const double Low = LowestFootZ(BindCs, Mesh->GetRefSkeleton(), Rot, K);
		if (Low < TNumericLimits<double>::Max() / 2)
			Comp->SetRelativeLocation(FVector(0, 0, -HalfHeight - Low));
		else
			Comp->SetRelativeLocation(FVector(0, 0, -HalfHeight - B.Min.Z * K));
	}
	UE_LOG(LogTemp, Log, TEXT("[Aerondel] FitRig %s: auto-align %.0f deg, scale %.5f, rot P%.1f Y%.1f R%.1f"),
		*Mesh->GetName(), FMath::RadiansToDegrees(Align.GetAngle()), K, Rot.Rotator().Pitch, Rot.Rotator().Yaw, Rot.Rotator().Roll);
	if (RefExtent <= KINDA_SMALL_NUMBER || !Comp->GetWorld()) return true;
	TWeakObjectPtr<USkeletalMeshComponent> Weak(Comp);
	auto Correct = [Weak, RefExtent, K, HalfHeight, Rot]()
	{
		USkeletalMeshComponent* C = Weak.Get();
		if (!C || !C->GetSkeletalMeshAsset()) return;
		C->TickAnimation(0.f, false);
		C->RefreshBoneTransforms();
		USkeletalMesh* M = C->GetSkeletalMeshAsset();
		const double Posed = UAerondelModelLibrary::BoneExtent(M, C);
		if (Posed <= KINDA_SMALL_NUMBER) return;
		double Ratio = Posed / RefExtent;
		FString Anim = TEXT("none");
		for (const FName& T : C->ComponentTags) if (T.ToString().StartsWith(TEXT("AerondelAnim:"))) Anim = T.ToString().Mid(13);
		{
			const FVector MS = M->GetBounds().GetBox().GetSize();
			UE_LOG(LogTemp, Log, TEXT("[Aerondel] RigDiag %s: mesh bounds %.1f x %.1f x %.1f, bind bone extent %.2f, posed bone extent %.2f (x%.2f), anim %s, fit scale %.5f"),
				*M->GetName(), MS.X, MS.Y, MS.Z, RefExtent, Posed, Posed / RefExtent, *Anim, K);
		}
		// A clip whose skeleton space does not match the mesh (or that still carries an extra
		// axis rotation) lays the body down or bunches the bones. Hold the bind pose instead.
		if (Anim != TEXT("bindpose"))
		{
			const FVector Up = PosedUp(M, C);
			const FVector WorldUp = Rot.RotateVector(Up);
			if (Ratio < 0.35 || (!Up.IsNearlyZero() && FVector::DotProduct(WorldUp, FVector::UpVector) < 0.5))
			{
				AerondelRigs::HoldRefPose(C, Ratio < 0.35 ? TEXT("posed skeleton bunched vs bind pose") : TEXT("clip laid the body down"));
				C->TickAnimation(0.f, false);
				C->RefreshBoneTransforms();
				return;
			}
		}
		if (Ratio > 0.7 && Ratio < 1.45) Ratio = 1.0;
		if (Ratio == 1.0) return;
		// glTF clips often carry a x100 armature scale the bind-pose bounds don't see.
		const double K2 = K / Ratio;
		C->SetRelativeScale3D(FVector(K2));
		const double Low = LowestFootZ(C->GetComponentSpaceTransforms(), M->GetRefSkeleton(), Rot, K2);
		if (Low < TNumericLimits<double>::Max() / 2)
			C->SetRelativeLocation(FVector(0, 0, -HalfHeight - Low));
		UE_LOG(LogTemp, Log, TEXT("[Aerondel] FitRig %s: posed x%.2f of bind pose, scale %.5f -> %.5f"), *M->GetName(), Ratio, K, K2);
	};
	Correct();
	Comp->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(Correct));
	return true;
}

double UAerondelModelLibrary::BoneExtent(const USkeletalMesh* Mesh, const USkeletalMeshComponent* Posed)
{
	// Largest axis extent of the bone positions (component space): reference pose, or the component's current pose.
	if (!Mesh) return 0.0;
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const int32 N = Ref.GetNum();
	if (N < 3) return 0.0;
	TArray<FTransform> Cs;
	if (Posed && Posed->GetComponentSpaceTransforms().Num() == N) Cs = Posed->GetComponentSpaceTransforms();
	else
	{
		const TArray<FTransform>& Local = Ref.GetRefBonePose();
		Cs.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			const int32 P = Ref.GetParentIndex(i);
			Cs[i] = P >= 0 ? Local[i] * Cs[P] : Local[i];
		}
	}
	FBox Box(ForceInit);
	for (const FTransform& T : Cs) Box += T.GetLocation();
	return Box.GetSize().GetMax();
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
