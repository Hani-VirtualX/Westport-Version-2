

#include "QuizAnswerRow.h"
#include "TrainingStepPage.h" 

void UQuizAnswerRow::NativeConstruct()
{
    Super::NativeConstruct();
    if (AnswerButton)
    {
        AnswerButton->OnPressed.AddDynamic(this, &UQuizAnswerRow::OnButtonPressed);
    }
}

void UQuizAnswerRow::NativeDestruct()
{
    if (AnswerButton)
    {
        AnswerButton->OnPressed.RemoveDynamic(this, &UQuizAnswerRow::OnButtonPressed);
    }
    Super::NativeDestruct();
}

// --- NEW: The C++ Animation Engine ---
void UQuizAnswerRow::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bIsFadingIn)
    {
        float CurrentOpacity = GetRenderOpacity();

        // Smoothly interpolate the opacity
        float NewOpacity = FMath::FInterpTo(CurrentOpacity, 1.0f, InDeltaTime, FadeSpeed);

        SetRenderOpacity(NewOpacity);

        // Lock it to 1.0 and shut down the math to save performance
        if (NewOpacity >= 0.99f)
        {
            SetRenderOpacity(1.0f);
            bIsFadingIn = false;
        }
    }
}

void UQuizAnswerRow::SetupRow(int32 InIndex, FText InAnswerText, UTrainingStepPage* InParent)
{
    AnswerIndex = InIndex;
    ParentPage = InParent;

    if (AnswerText)
    {
        AnswerText->SetText(InAnswerText);
    }

    SetRowState(EQuizAnswerState::Idle);

    // --- NEW: Start invisible so the NativeTick can fade it in ---
    SetRenderOpacity(0.0f);
    bIsFadingIn = true;
}

void UQuizAnswerRow::OnButtonPressed()
{
    if (ParentPage && AnswerIndex != -1)
    {
        ParentPage->SelectQuizAnswer(AnswerIndex);
    }
}

void UQuizAnswerRow::SetRowState(EQuizAnswerState NewState)
{
    CurrentState = NewState;

    if (CurrentState == EQuizAnswerState::Correct || CurrentState == EQuizAnswerState::Wrong)
    {
        if (ResultIcon) ResultIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (ArrowTickIndicator) ArrowTickIndicator->SetVisibility(ESlateVisibility::Hidden);
    }
    else
    {
        if (ResultIcon) ResultIcon->SetVisibility(ESlateVisibility::Hidden);
        if (ArrowTickIndicator) ArrowTickIndicator->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    OnStateChanged(CurrentState);
}

void UQuizAnswerRow::SetButtonEnabled(bool bInEnabled)
{
    if (AnswerButton)
    {
        AnswerButton->SetIsEnabled(bInEnabled);
    }
}