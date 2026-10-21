#include "World/GielinorScalability.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/IConsoleManager.h"

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
		break;
	case EGielinorTier::Low:
		S->SetOverallScalabilityLevel(1);
		SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), 0); // no Lumen GI
		SetCVar(TEXT("r.ReflectionMethod"), 2);                // SSR
		SetCVar(TEXT("r.Shadow.Virtual.Enable"), 0);
		break;
	case EGielinorTier::High:
		S->SetOverallScalabilityLevel(2);
		SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), 1); // Lumen
		SetCVar(TEXT("r.ReflectionMethod"), 1);
		SetCVar(TEXT("r.Shadow.Virtual.Enable"), 1);
		break;
	case EGielinorTier::Ultra:
		S->SetOverallScalabilityLevel(4); // Cinematic
		SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), 1);
		SetCVar(TEXT("r.ReflectionMethod"), 1);
		SetCVar(TEXT("r.Shadow.Virtual.Enable"), 1);
		SetCVar(TEXT("r.Lumen.HardwareRayTracing"), 1);
		break;
	}
	S->ApplySettings(false);
}
