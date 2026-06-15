

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FlowControlLibrary.generated.h"

// 1. Define the Input Mode
UENUM(BlueprintType)
enum class EFlowMode : uint8
{
    Interaction     UMETA(DisplayName = "Interaction (Mandatory)"),
    Explanation     UMETA(DisplayName = "Explanation (Conditional)")
};

// 2. Define the Output Pins
UENUM(BlueprintType)
enum class EFlowResult : uint8
{
    RunStep         UMETA(DisplayName = "Run Step"),
    Skip            UMETA(DisplayName = "Skip / Continue")
};

UCLASS()
class AZUREAL_CSM_API UFlowControlLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    /**
     * Controls flow based on Mode.
     * @param Mode - Interaction triggers 'Run Step'. Explanation checks the boolean.
     * @param bExplanationCondition - Only used if Mode is Explanation.
     * @param OutBranch - The output execution path.
     */
    UFUNCTION(BlueprintCallable, Category = "Flow Control", meta = (ExpandEnumAsExecs = "OutBranch"))
    static void FlowController(EFlowMode Mode, bool bExplanationCondition, EFlowResult& OutBranch);
};