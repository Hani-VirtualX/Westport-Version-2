

#include "FlowControlLibrary.h"

void UFlowControlLibrary::FlowController(EFlowMode Mode, bool bExplanationCondition, EFlowResult& OutBranch)
{
    // Logic 1: Interaction Mode (Mandatory)
    if (Mode == EFlowMode::Interaction)
    {
        // Always run the step, never skip
        OutBranch = EFlowResult::RunStep;
        return;
    }

    // Logic 2: Explanation Mode (Conditional)
    if (Mode == EFlowMode::Explanation)
    {
        if (bExplanationCondition == true)
        {
            // Condition met, run the step
            OutBranch = EFlowResult::RunStep;
        }
        else
        {
            // Condition failed, skip immediately to output
            OutBranch = EFlowResult::Skip;
        }
    }
}