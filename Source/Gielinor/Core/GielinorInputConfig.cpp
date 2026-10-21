#include "Core/GielinorInputConfig.h"

#define GIELINOR_IA(Name) TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Gielinor/Input/" Name "." Name)))

UGielinorInputConfig::UGielinorInputConfig()
{
	DesktopContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Gielinor/Input/IMC_Desktop.IMC_Desktop")));
	XRContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/Gielinor/Input/IMC_XR.IMC_XR")));
	Move = GIELINOR_IA("IA_Move");
	Look = GIELINOR_IA("IA_Look");
	OrbitHold = GIELINOR_IA("IA_OrbitHold");
	Zoom = GIELINOR_IA("IA_Zoom");
	Click = GIELINOR_IA("IA_Click");
	Interact = GIELINOR_IA("IA_Interact");
	Jump = GIELINOR_IA("IA_Jump");
	ToggleInventory = GIELINOR_IA("IA_ToggleInventory");
	XRMove = GIELINOR_IA("IA_XR_Move");
	XRTurn = GIELINOR_IA("IA_XR_Turn");
	XRTeleport = GIELINOR_IA("IA_XR_Teleport");
	XRGrabLeft = GIELINOR_IA("IA_XR_GrabLeft");
	XRGrabRight = GIELINOR_IA("IA_XR_GrabRight");
	XRTriggerLeft = GIELINOR_IA("IA_XR_TriggerLeft");
	XRTriggerRight = GIELINOR_IA("IA_XR_TriggerRight");
	XRMenu = GIELINOR_IA("IA_XR_Menu");
}

#undef GIELINOR_IA
