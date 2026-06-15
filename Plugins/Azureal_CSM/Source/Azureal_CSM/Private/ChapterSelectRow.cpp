

#include "ChapterSelectRow.h"

void UChapterSelectRow::SetupSpecificRow(int32 Index, FText Title, int32 CurrentSteps, int32 MaxSteps, bool bIsComplete, bool bIsCurrentLocation)
{
    AssignedIndex = Index;

    // 1. Setup Texts
    if (ChapterNumberText) ChapterNumberText->SetText(FText::AsNumber(Index + 1));
    if (ChapterTitleText) ChapterTitleText->SetText(Title);

    if (ProgressText) {
        FString ProgressStr = FString::Printf(TEXT("%d / %d"), CurrentSteps, MaxSteps);
        ProgressText->SetText(FText::FromString(ProgressStr));
    }

    // --- CRITICAL FIX: SWAPPED LOGIC ORDER ---
    // We check "IsComplete" FIRST.
    // This ensures that when the Result Page appears, the icon turns "Completed" immediately,
    // even though we are technically still inside the level (bIsCurrentLocation is true).

    EChapterCompletionState CurrentState = EChapterCompletionState::Incomplete;

    if (bIsComplete)
    {
        // Priority 1: If memory says done, it is "Completed" (Green Check)
        CurrentState = EChapterCompletionState::Completed;
    }
    else if (bIsCurrentLocation)
    {
        // Priority 2: If not done, but we are here, it is "In Progress" (Yellow/Hourglass)
        CurrentState = EChapterCompletionState::InProgress;
    }
    else
    {
        // Priority 3: Otherwise, it is "Incomplete" (Grey/Lock)
        CurrentState = EChapterCompletionState::Incomplete;
    }

    // Trigger the Blueprint Event with the Enum
    UpdateCompletionImage(CurrentState);

    // --- NAVIGATION STATE LOGIC (Stays the same) ---
    // This handles the "You Are Here" text/selection, which is separate from the Completion Icon.
    if (bIsCurrentLocation)
    {
        SetRowState(EChapterState::YouAreHere);
        if (ChapterButton) ChapterButton->SetIsEnabled(false);
    }
    else
    {
        SetRowState(EChapterState::Idle);
        if (ChapterButton) ChapterButton->SetIsEnabled(true);
    }
}

void UChapterSelectRow::SetRowState(EChapterState NewState)
{
    OnStateChanged(NewState);
}

void UChapterSelectRow::BroadcastClick()
{
    if (OnRowClicked.IsBound())
    {
        OnRowClicked.Broadcast(AssignedIndex);
    }
}