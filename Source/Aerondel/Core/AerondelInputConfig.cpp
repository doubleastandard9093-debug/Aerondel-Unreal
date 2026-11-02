#include "Core/AerondelInputConfig.h"

#define AERONDEL_IA(Name) TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Gielinor/Input/" Name "." Name)))

UAerondelInputConfig::UAerondelInputConfig()
{
	DesktopContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Gielinor/Input/IMC_Desktop.IMC_Desktop")));
	XRContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Gielinor/Input/IMC_XR.IMC_XR")));
	Move = AERONDEL_IA("IA_Move");
	Look = AERONDEL_IA("IA_Look");
	OrbitHold = AERONDEL_IA("IA_OrbitHold");
	Zoom = AERONDEL_IA("IA_Zoom");
	Click = AERONDEL_IA("IA_Click");
	Interact = AERONDEL_IA("IA_Interact");
	Jump = AERONDEL_IA("IA_Jump");
	ToggleInventory = AERONDEL_IA("IA_ToggleInventory");
	XRMove = AERONDEL_IA("IA_XR_Move");
	XRTurn = AERONDEL_IA("IA_XR_Turn");
	XRTeleport = AERONDEL_IA("IA_XR_Teleport");
	XRGrabLeft = AERONDEL_IA("IA_XR_GrabLeft");
	XRGrabRight = AERONDEL_IA("IA_XR_GrabRight");
	XRTriggerLeft = AERONDEL_IA("IA_XR_TriggerLeft");
	XRTriggerRight = AERONDEL_IA("IA_XR_TriggerRight");
	XRMenu = AERONDEL_IA("IA_XR_Menu");
}

#undef AERONDEL_IA
