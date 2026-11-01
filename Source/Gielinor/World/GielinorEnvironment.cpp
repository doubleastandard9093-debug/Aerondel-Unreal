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
			// High/Ultra: screen-space contact shadows ground props and characters (off on Low: r.ContactShadows=0 anyway).
			Sun->ContactShadowLength = bRich ? (bUltra ? 0.08f : 0.05f) : 0.f;
			Sun->MarkRenderStateDirty();
		}
		if (USkyLightComponent* Sky = It->FindComponentByClass<USkyLightComponent>())
		{
			Sky->bRealTimeCapture = bRich; // real-time sky capture follows the atmosphere + clouds
			Sky->SetCastShadows(true);
			Sky->bLowerHemisphereIsBlack = false; // ground bounce tint instead of black undersides on round props
			Sky->RecaptureSky();
		}
		if (UExponentialHeightFogComponent* Fog = It->FindComponentByClass<UExponentialHeightFogComponent>())
		{
			Fog->SetVolumetricFog(bRich);
			Fog->SetVolumetricFogScatteringDistribution(0.6f);
			Fog->SetVolumetricFogExtinctionScale(bUltra ? 1.2f : 1.f);
			Fog->SetFogDensity(bRich ? 0.012f : 0.01f);
			Fog->SetFogHeightFalloff(bRich ? 0.15f : 0.2f);
			// Cool, slightly blue distance haze on High/Ultra for aerial depth; volumetric fog picks up sun shafts.
			Fog->SetFogInscatteringColor(bRich ? FLinearColor(0.42f, 0.52f, 0.66f) : FLinearColor(0.45f, 0.53f, 0.62f));
			Fog->SetVolumetricFogDistance(bUltra ? 9000.f : 6000.f);
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
		P.bOverride_AmbientOcclusionQuality = true;   P.AmbientOcclusionQuality = bRich ? (bUltra ? 100.f : 75.f) : 40.f;
		P.bOverride_AutoExposureMethod = true;        P.AutoExposureMethod = bRich ? EAutoExposureMethod::AEM_Histogram : EAutoExposureMethod::AEM_Basic;
		P.bOverride_AutoExposureSpeedUp = true;       P.AutoExposureSpeedUp = 2.f;   // eyes adjust walking into/out of houses and caves
		P.bOverride_AutoExposureSpeedDown = true;     P.AutoExposureSpeedDown = 1.f;
		P.bOverride_BloomThreshold = true;            P.BloomThreshold = -1.f;
		P.bOverride_SceneFringeIntensity = true;      P.SceneFringeIntensity = 0.f;  // no chromatic fringe on any tier
		P.bOverride_ColorGamma = true;                P.ColorGamma = FVector4(1, 1, 1, 1);
		P.bOverride_WhiteTemp = true;                 P.WhiteTemp = bRich ? 6300.f : 6500.f; // touch warmer daylight on High/Ultra
		P.bOverride_LumenSkylightLeaking = true;      P.LumenSkylightLeaking = bRich ? 0.05f : 0.f; // interiors never go pitch black
		P.bOverride_LumenMaxTraceDistance = true;     P.LumenMaxTraceDistance = bUltra ? 20000.f : 10000.f;
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
