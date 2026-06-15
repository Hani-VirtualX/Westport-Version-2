

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Azr_HandAnimInstance.generated.h"

/**
 * C++ Base for Hand Animations.
 * Direct communication line from Pawn to AnimGraph.
 */
UCLASS()
class AZUREALXR_API UAzr_HandAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// The variable the AnimGraph will read (0.0 = Open, 1.0 = Closed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Azureal|Hands")
	float GripAlpha;
};