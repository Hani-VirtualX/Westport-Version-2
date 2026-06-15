

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ExplanationFlowLibrary.generated.h"

// Define the Output Pins for the Controller
UENUM(BlueprintType)
enum class EExplanationResult : uint8
{
    RunStep     UMETA(DisplayName = "Run Step"),
    Skip        UMETA(DisplayName = "Skip")
};

UCLASS()
class AZUREAL_CSM_API UExplanationFlowLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

private:
    // The hidden global variable that stores the state
    static bool bGlobalExplanationEnabled;

public:
    /**
     * Set this once (e.g., in GameInstance or BeginPlay) to toggle
     * all Explanation Flow Controller nodes globally.
     */
    UFUNCTION(BlueprintCallable, Category = "Explanation System")
    static void SetExplanationBoolean(bool bEnable);

    /**
     * NEW: Returns the current state of the global boolean.
     * Used by ChapterSubsystem to calculate total steps correctly.
     */
    UFUNCTION(BlueprintPure, Category = "Explanation System")
    static bool IsExplanationEnabled();

    /**
     * Checks the global boolean.
     * If True -> Fires 'Run Step'.
     * If False -> Fires 'Skip'.
     */
    UFUNCTION(BlueprintCallable, Category = "Explanation System", meta = (ExpandEnumAsExecs = "OutBranch"))
    static void ExplanationFlowController(EExplanationResult& OutBranch);
};