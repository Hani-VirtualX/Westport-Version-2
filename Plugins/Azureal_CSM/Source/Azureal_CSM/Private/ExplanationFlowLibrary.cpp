

#include "ExplanationFlowLibrary.h"

// Initialize the static variable (Defaulting to true so explanations show by default)
bool UExplanationFlowLibrary::bGlobalExplanationEnabled = true;

void UExplanationFlowLibrary::SetExplanationBoolean(bool bEnable)
{
    // Update the global state
    bGlobalExplanationEnabled = bEnable;
}

bool UExplanationFlowLibrary::IsExplanationEnabled()
{
    return bGlobalExplanationEnabled;
}

void UExplanationFlowLibrary::ExplanationFlowController(EExplanationResult& OutBranch)
{
    // Check the global state
    if (bGlobalExplanationEnabled)
    {
        OutBranch = EExplanationResult::RunStep;
    }
    else
    {
        OutBranch = EExplanationResult::Skip;
    }
}