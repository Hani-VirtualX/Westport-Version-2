

#include "TrainingCurriculum.h"

TArray<FRuntimeStep> UTrainingCurriculum::GetFilteredSteps(bool bShowExplanations)
{
    TArray<FRuntimeStep> Result;
    int32 CurrentStepIndex = 1;

    for (const FStepData& RawStep : MasterSteps)
    {
        FRuntimeStep NewRuntimeStep;
        NewRuntimeStep.StepType = RawStep.StepType;

        // --- BRANCH A: Handle Quiz Compilation ---
        if (RawStep.StepType == EMasterStepType::Quiz)
        {
            NewRuntimeStep.StepTitle = RawStep.QuizTitle; // Map Question to StepTitle
            NewRuntimeStep.CorrectAnswerIndex = RawStep.CorrectAnswerIndex;

            // Convert our clean Quiz Answers into standard Runtime Substeps behind the scenes
            for (const FQuizAnswerData& Ans : RawStep.QuizAnswers)
            {
                FSubStepData DummySub;
                DummySub.Description = Ans.AnswerText;
                DummySub.Type = EStepType::Interaction; // Quizzes are always mandatory interaction
                NewRuntimeStep.ActiveSubSteps.Add(DummySub);
            }
        }
        // --- BRANCH B: Handle Standard Compilation ---
        else
        {
            NewRuntimeStep.StepTitle = RawStep.StepTitle;
            NewRuntimeStep.CorrectAnswerIndex = 0;

            for (const FSubStepData& Sub : RawStep.SubSteps)
            {
                if (Sub.Type == EStepType::Explanation && !bShowExplanations)
                {
                    continue; // Skip explanation if disabled
                }
                NewRuntimeStep.ActiveSubSteps.Add(Sub);
            }
        }

        // Only commit the step if it has content
        if (NewRuntimeStep.ActiveSubSteps.Num() > 0)
        {
            NewRuntimeStep.DisplayNumber = CurrentStepIndex;
            Result.Add(NewRuntimeStep);
            CurrentStepIndex++;
        }
    }
    return Result;
}

int32 UTrainingCurriculum::CountFilteredSteps(bool bShowExplanations) const
{
    int32 Count = 0;

    for (const FStepData& RawStep : MasterSteps)
    {
        if (RawStep.StepType == EMasterStepType::Quiz)
        {
            if (RawStep.QuizAnswers.Num() > 0) Count++;
            continue;
        }

        bool bHasValidContent = false;
        for (const FSubStepData& Sub : RawStep.SubSteps)
        {
            if (Sub.Type == EStepType::Explanation && !bShowExplanations) continue;
            bHasValidContent = true;
            break;
        }

        if (bHasValidContent) Count++;
    }
    return Count;
}