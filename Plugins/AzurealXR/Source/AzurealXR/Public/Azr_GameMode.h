

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Azr_GameMode.generated.h"

/**
 * The Framework Manager.
 * Defines the Default Pawn (Your VR Body) and Player Controller.
 */
UCLASS()
class AZUREALXR_API AAzr_GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAzr_GameMode();
};