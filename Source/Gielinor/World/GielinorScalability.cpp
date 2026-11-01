#include "World/GielinorScalability.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"

static const TCHAR* GTierSection = TEXT("Gielinor");
static const TCHAR* GTierKey = TEXT("GraphicsTier");

static void SetCVar(const TCHAR* Name, int32 V)
{
	if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(Name)) C->Set(V, ECVF_SetByGameSetting);
}

EGielinorTier UGielinorScalability::DefaultTier()
{
#if PLATFORM_ANDROID
	return EGielinorTier::Quest;
#else
	return EGielinorTier::Low;
#endif
}

EGielinorTier UGielinorScalability::GetSavedTier()
{
	int32 V = -1;
	if (GConfig && GConfig->GetInt(GTierSection, GTierKey, V, GGameUserSettingsIni) && V >= 0 && V <= (int32)EGielinorTier::Ultra) return (EGielinorTier)V;
	return DefaultTier();
}

void UGielinorScalability::SetAndSaveTier(const UObject* WorldContext, EGielinorTier Tier)
{
	ApplyTier(WorldContext, Tier);
	if (GConfig)
	{
		GConfig->SetInt(GTierSection, GTierKey, (int32)Tier, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

bool UGielinorScalability::GetNaniteOptIn()
{
	bool b = false;
	if (GConfig) GConfig->GetBool(GTierSection, TEXT("NaniteOptIn"), b, GGameUserSettingsIni);
	return b;
}

void UGielinorScalability::SetNaniteOptIn(bool bOn)
{
	if (!GConfig) return;
	GConfig->SetBool(GTierSection, TEXT("NaniteOptIn"), bOn, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UGielinorScalability::ApplySavedTier(const UObject* WorldContext) { ApplyTier(WorldContext, GetSavedTier()); }

FOnGielinorTierApplied UGielinorScalability::OnTierApplied;
static EGielinorTier GAppliedTier = EGielinorTier::Low;
EGielinorTier UGielinorScalability::GetAppliedTier() { return GAppliedTier; }

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
	SetCVar(TEXT("r.Nanite"), UGielinorScalability::GetNaniteOptIn() ? 1 : 0);
}

void UGielinorScalability::ApplyTier(const UObject* WorldContext, EGielinorTier Tier)
{
#if PLATFORM_ANDROID
	Tier = EGielinorTier::Quest;
#endif
	UGameUserSettings* S = UGameUserSettings::GetGameUserSettings();
	if (!S) return;
	switch (Tier)
	{
	case EGielinorTier::Quest:
		S->SetOverallScalabilityLevel(0);
		SetCVar(TEXT("r.ScreenPercentage"), 100);
		ApplyCheapPath();
		SetCVar(TEXT("r.AntiAliasingMethod"), 1); // FXAA (not allowed in Scalability.ini)
		break;
	case EGielinorTier::Low:
		S->SetOverallScalabilityLevel(1);
		ApplyCheapPath();
		SetCVar(TEXT("r.AntiAliasingMethod"), 4); // TSR: smoothest edges on round props
		// Render at 75% and let TSR upscale: smooth edges on the i3/UHD without the full-res cost.
		SetCVar(TEXT("r.ScreenPercentage"), 75);
		SetCVar(TEXT("r.MaxAnisotropy"), 4); // also set by TextureQuality@1; repeated so a custom ini can't drop it
		break;
	case EGielinorTier::High:
		S->SetOverallScalabilityLevel(2);
		SetCVar(TEXT("r.ScreenPercentage"), 100);
		ApplyRichPath(false);
		break;
	case EGielinorTier::Ultra:
		S->SetOverallScalabilityLevel(3); // Epic (Cinematic is for offline renders)
		SetCVar(TEXT("r.ScreenPercentage"), 100);
		ApplyRichPath(true);
		break;
	}
	S->ApplySettings(false);
	GAppliedTier = Tier;
	OnTierApplied.Broadcast(Tier);
}
