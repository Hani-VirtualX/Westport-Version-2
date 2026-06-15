

// Fill out your copyright notice in the Description page of Project Settings.

#include "TrainingSubStepRow.h"

void UTrainingSubStepRow::NativeConstruct()
{
    Super::NativeConstruct();

    // Start invisible the exact moment Blueprint creates the widget!
    SetRenderOpacity(0.0f);
    bIsFadingIn = true;
}

void UTrainingSubStepRow::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Run the fade animation
    if (bIsFadingIn)
    {
        float CurrentOpacity = GetRenderOpacity();
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

void UTrainingSubStepRow::SetState(ESubStepState NewState)
{
    // Simple optimization to avoid redrawing if not needed
    if (CurrentState == NewState) return;

    CurrentState = NewState;
    OnStateChanged(NewState);
}