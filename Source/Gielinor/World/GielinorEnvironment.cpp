#include "World/GielinorEnvironment.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Engine/World.h"

// Engine-provided cloud material (ships with UE). Only resolved on High/Ultra.
static const TCHAR* GCloudMaterial = TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst");

AGielinorEnvironment::AGielinorEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AGielinorEnvironment::BeginPlay()
{
	Super::BeginPlay();
	TierHandle = UGielinorScalability::OnTierApplied.AddUObject(this, &AGielinorEnvironment::Apply);
	Apply(UGielinorScalability::GetAppliedTier());
}

void AGielinorEnvironment::EndPlay(const EEndPlayReason::Type Reason)
{
	UGielinorScalability::OnTierApplied.Remove(TierHandle);
	Super::EndPlay(Reason);
}

void AGielinorEnvironment::Apply(EGielinorTier Tier)
{
	UWorld* W = GetWorld();
	if (!W) return;
	const bool bRich = Tier >= EGielinorTier::High, bUltra = Tier == EGielinorTier::Ultra;

	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (UDirectionalLightComponent* Sun = It->FindComponentByClass<UDirectionalLightComponent>())
		{
			Sun->SetAtmosphereSunLight(true);
			Sun->bEnableLightShaftBloom = bRich;
			Sun->bCastCloudShadows = bRich;
			Sun->SetDynamicShadowDistanceMovableLight(bRich ? 12000.f : 6000.f);
			Sun->SetDynamicShadowCascades(bRich ? 4 : 2);
			// Wider sun disc = softer shadow penumbra; on Low this hides shadow-map stair-steps around round props.
			Sun->LightSourceAngle = bRich ? 0.8f : 1.2f;
			Sun->CascadeDistributionExponent = bRich ? 3.f : 2.4f; // more near-cascade texels = smoother close shadows
			Sun->MarkRenderStateDirty();
		}
		if (USkyLightComponent* Sky = It->FindComponentByClass<USkyLightComponent>())
		{
			Sky->bRealTimeCapture = bRich; // real-time sky capture follows the atmosphere + clouds
			Sky->SetCastShadows(true);
			Sky->RecaptureSky();
		}
		if (UExponentialHeightFogComponent* Fog = It->FindComponentByClass<UExponentialHeightFogComponent>())
		{
			Fog->SetVolumetricFog(bRich);
			Fog->SetVolumetricFogScatteringDistribution(0.6f);
			Fog->SetVolumetricFogExtinctionScale(bUltra ? 1.2f : 1.f);
			Fog->SetFogDensity(bRich ? 0.012f : 0.01f);
			Fog->SetFogHeightFalloff(bRich ? 0.15f : 0.2f);
		}
		if (USkyAtmosphereComponent* Atmo = It->FindComponentByClass<USkyAtmosphereComponent>())
			Atmo->SetAerialPespectiveViewDistanceScale(bRich ? 1.f : 0.6f);
	}

	for (TActorIterator<APostProcessVolume> It(W); It; ++It)
	{
		if (!It->bUnbound) continue;
		FPostProcessSettings& P = It->Settings;
		P.bOverride_BloomIntensity = true;            P.BloomIntensity = bRich ? 0.8f : 0.4f;
		P.bOverride_AmbientOcclusionIntensity = true; P.AmbientOcclusionIntensity = bRich ? 0.6f : 0.5f; // Low: soft SSAO makes greybox read round
		P.bOverride_AmbientOcclusionRadius = true;    P.AmbientOcclusionRadius = bRich ? 120.f : 85.f;
		P.bOverride_VignetteIntensity = true;         P.VignetteIntensity = bRich ? 0.3f : 0.f;
		P.bOverride_ColorSaturation = true;           P.ColorSaturation = bRich ? FVector4(1.06f, 1.06f, 1.06f, 1.f) : FVector4(1, 1, 1, 1);
		P.bOverride_ColorContrast = true;             P.ColorContrast = bRich ? FVector4(1.04f, 1.04f, 1.04f, 1.f) : FVector4(1, 1, 1, 1);
		P.bOverride_FilmToe = true;                   P.FilmToe = bRich ? 0.6f : 0.55f;
		P.bOverride_LensFlareIntensity = true;        P.LensFlareIntensity = bRich ? 0.15f : 0.f;
		P.bOverride_DynamicGlobalIlluminationMethod = true;
		P.DynamicGlobalIlluminationMethod = bRich ? EDynamicGlobalIlluminationMethod::Lumen : EDynamicGlobalIlluminationMethod::None;
		P.bOverride_ReflectionMethod = true;
		P.ReflectionMethod = bRich ? EReflectionMethod::Lumen : EReflectionMethod::ScreenSpace;
		P.bOverride_LumenSceneLightingQuality = true; P.LumenSceneLightingQuality = bUltra ? 2.f : 1.f;
		P.bOverride_LumenFinalGatherQuality = true;   P.LumenFinalGatherQuality = bUltra ? 2.f : 1.f;
		P.bOverride_LumenReflectionQuality = true;    P.LumenReflectionQuality = bUltra ? 2.f : 1.f;
		P.bOverride_LumenSceneDetail = true;          P.LumenSceneDetail = bUltra ? 2.f : 1.f;
	}

	if (bRich && !Clouds)
	{
		if (UMaterialInterface* CloudMat = LoadObject<UMaterialInterface>(nullptr, GCloudMaterial))
		{
			Clouds = NewObject<UVolumetricCloudComponent>(this, TEXT("Clouds"));
			Clouds->SetupAttachment(RootComponent);
			Clouds->SetMaterial(CloudMat);
			Clouds->SetLayerBottomAltitude(5.f);   // km
			Clouds->SetLayerHeight(6.f);
			Clouds->SetTracingMaxDistance(bUltra ? 80.f : 50.f);
			Clouds->RegisterComponent();
		}
	}
	else if (!bRich && Clouds)
	{
		Clouds->DestroyComponent();
		Clouds = nullptr;
	}
}
