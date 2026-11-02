#include "World/AerondelScalability.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

static const TCHAR* GTierSection = TEXT("Gielinor");
static const TCHAR* GTierKey = TEXT("GraphicsTier");

static void SetCVar(const TCHAR* Name, int32 V)
{
	if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(Name)) C->Set(V, ECVF_SetByGameSetting);
}

EAerondelTier UAerondelScalability::DefaultTier()
{
#if PLATFORM_ANDROID
	return EAerondelTier::Quest;
#else
	return EAerondelTier::Low;
#endif
}

EAerondelTier UAerondelScalability::GetSavedTier()
{
	int32 V = -1;
	if (GConfig && GConfig->GetInt(GTierSection, GTierKey, V, GGameUserSettingsIni) && V >= 0 && V <= (int32)EAerondelTier::Ultra) return (EAerondelTier)V;
	return DefaultTier();
}

void UAerondelScalability::SetAndSaveTier(const UObject* WorldContext, EAerondelTier Tier)
{
	ApplyTier(WorldContext, Tier);
	if (GConfig)
	{
		GConfig->SetInt(GTierSection, GTierKey, (int32)Tier, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

bool UAerondelScalability::GetNaniteOptIn()
{
	bool b = false;
	if (GConfig) GConfig->GetBool(GTierSection, TEXT("NaniteOptIn"), b, GGameUserSettingsIni);
	return b;
}

void UAerondelScalability::SetNaniteOptIn(bool bOn)
{
	if (!GConfig) return;
	GConfig->SetBool(GTierSection, TEXT("NaniteOptIn"), bOn, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UAerondelScalability::ApplySavedTier(const UObject* WorldContext) { ApplyTier(WorldContext, GetSavedTier()); }

FOnAerondelTierApplied UAerondelScalability::OnTierApplied;
static EAerondelTier GAppliedTier = EAerondelTier::Low;
EAerondelTier UAerondelScalability::GetAppliedTier() { return GAppliedTier; }

/**
 * World Partition streaming per tier (World map only; non-partitioned maps ignore it).
 * The map's runtime grid is 128 m cells / 384 m range (world_partition.py). Lower tiers
 * shrink the loading range and load fewer cells at once so memory and hitching stay low
 * on the 8 GB laptop and on Android. Unknown cvars/commands are harmless no-ops.
 */
static void ApplyWorldStreaming(const UObject* WorldContext, int32 RangeCm, int32 MaxLoadingCells)
{
	SetCVar(TEXT("wp.Runtime.MaxLoadingStreamingCells"), MaxLoadingCells);
	UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (World && World->GetWorldPartition())
	{
		GEngine->Exec(World, *FString::Printf(TEXT("wp.Runtime.OverrideRuntimeSpatialHashLoadingRange -grid=0 -range=%d"), RangeCm));
	}
}

/** Strict cheap path shared by Quest and Low: no Lumen, no VSM, no volumetrics, no Nanite, nothing that pulls High content. */
static void ApplyCheapPath()
{
	SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), 0); // no Lumen GI
	SetCVar(TEXT("r.ReflectionMethod"), 2);                // SSR
	SetCVar(TEXT("r.Shadow.Virtual.Enable"), 0);
	SetCVar(TEXT("r.VolumetricFog"), 0);
	SetCVar(TEXT("r.VolumetricCloud"), 0);
	SetCVar(TEXT("r.Lumen.HardwareRayTracing"), 0);
	SetCVar(TEXT("r.DistanceFieldAO"), 0);
	SetCVar(TEXT("r.ContactShadows"), 0);
	SetCVar(TEXT("r.SSS.Quality"), 0);
	SetCVar(TEXT("r.Nanite"), 0);
}

/** High / Ultra: Lumen GI + reflections, VSM, volumetric fog and clouds, DFAO, contact shadows. */
static void ApplyRichPath(bool bUltra)
{
	SetCVar(TEXT("r.AntiAliasingMethod"), 4); // TSR (not allowed in Scalability.ini)
	SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), 1);
	SetCVar(TEXT("r.ReflectionMethod"), 1);
	SetCVar(TEXT("r.Lumen.Reflections.Allow"), 1);
	SetCVar(TEXT("r.Lumen.TraceMeshSDFs"), 1);
	SetCVar(TEXT("r.Shadow.Virtual.Enable"), 1);
	SetCVar(TEXT("r.VolumetricFog"), 1);
	SetCVar(TEXT("r.VolumetricFog.GridPixelSize"), bUltra ? 8 : 16);
	SetCVar(TEXT("r.VolumetricCloud"), 1);
	SetCVar(TEXT("r.DistanceFieldAO"), 1);
	SetCVar(TEXT("r.ContactShadows"), 1);
	SetCVar(TEXT("r.SSS.Quality"), 1);
	SetCVar(TEXT("r.Lumen.HardwareRayTracing"), 0); // software Lumen: works on DX11/SM5 fallback too
	// Nanite stays off (crash fix 9b07fa6) unless the player opted in from Settings.
	SetCVar(TEXT("r.Nanite"), UAerondelScalability::GetNaniteOptIn() ? 1 : 0);
}

void UAerondelScalability::ApplyTier(const UObject* WorldContext, EAerondelTier Tier)
{
#if PLATFORM_ANDROID
	Tier = EAerondelTier::Quest;
#endif
	UGameUserSettings* S = UGameUserSettings::GetGameUserSettings();
	if (!S) return;
	switch (Tier)
	{
	case EAerondelTier::Quest:
		S->SetOverallScalabilityLevel(0);
		SetCVar(TEXT("r.ScreenPercentage"), 100);
		ApplyCheapPath();
		SetCVar(TEXT("r.AntiAliasingMethod"), 1); // FXAA (not allowed in Scalability.ini)
		ApplyWorldStreaming(WorldContext, 16000, 2); // 160 m
		break;
	case EAerondelTier::Low:
		S->SetOverallScalabilityLevel(1);
		ApplyCheapPath();
		SetCVar(TEXT("r.AntiAliasingMethod"), 4); // TSR: smoothest edges on round props
		// Render at 75% and let TSR upscale: smooth edges on the i3/UHD without the full-res cost.
		SetCVar(TEXT("r.ScreenPercentage"), 75);
		SetCVar(TEXT("r.MaxAnisotropy"), 4); // also set by TextureQuality@1; repeated so a custom ini can't drop it
		ApplyWorldStreaming(WorldContext, 25600, 2); // 256 m
		break;
	case EAerondelTier::High:
		S->SetOverallScalabilityLevel(2);
		SetCVar(TEXT("r.ScreenPercentage"), 100);
		ApplyRichPath(false);
		ApplyWorldStreaming(WorldContext, 38400, 4); // 384 m (map default)
		break;
	case EAerondelTier::Ultra:
		S->SetOverallScalabilityLevel(3); // Epic (Cinematic is for offline renders)
		SetCVar(TEXT("r.ScreenPercentage"), 100);
		ApplyRichPath(true);
		ApplyWorldStreaming(WorldContext, 51200, 4); // 512 m
		break;
	}
	S->ApplySettings(false);
	GAppliedTier = Tier;
	OnTierApplied.Broadcast(Tier);
}
