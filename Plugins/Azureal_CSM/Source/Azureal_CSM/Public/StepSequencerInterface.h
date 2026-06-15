

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "StepSequencerInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UStepSequencerInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Implement this interface on your Chapter Game Manager Blueprints.
 */
class AZUREAL_CSM_API IStepSequencerInterface
{
    GENERATED_BODY()

public:
    /** * Fires the step logic in your Game Manager.
     * In Blueprint, implement "Event Run Steps Order".
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Sequencer")
    void RunStepsOrder();
};