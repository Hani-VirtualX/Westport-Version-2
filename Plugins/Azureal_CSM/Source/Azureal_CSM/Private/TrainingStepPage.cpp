

#include "TrainingStepPage.h"
#include "ChapterSubsystem.h"
#include "TrainingCurriculum.h"
#include "ExplanationFlowLibrary.h"
#include "TrainingSubStepRow.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h" 
#include "QuizAnswerRow.h"
#include "Engine/GameInstance.h"

// --- SETUP ---

void UTrainingStepPage::InitializePageData()
{
    InitRetryCount = 0;
    TryInitializeData();
}

void UTrainingStepPage::TryInitializeData()
{
    if (!GetWorld()) return;

    UGameInstance* GI = GetGameInstance();
    UChapterSubsystem* Subsystem = nullptr;
    if (GI) Subsystem = GI->GetSubsystem<UChapterSubsystem>();

    bool bIsReady = false;
    UTrainingCurriculum* Data = nullptr;

    if (Subsystem)
    {
        Data = Subsystem->GetCurrentStepData();
        if (Data) bIsReady = true;
    }

    if (!bIsReady)
    {
        if (InitRetryCount < 10)
        {
            InitRetryCount++;
            GetWorld()->GetTimerManager().SetTimer(InitRetryTimer, this, &UTrainingStepPage::TryInitializeData, 0.1f, false);
        }
        return;
    }

    GetWorld()->GetTimerManager().ClearTimer(InitRetryTimer);
    SessionSteps.Empty();

    bool bShowExpl = UExplanationFlowLibrary::IsExplanationEnabled();
    SessionSteps = Data->GetFilteredSteps(bShowExpl);

    SetTotalSessionSteps(SessionSteps.Num());
    OnPageChanged(0);

    // NEW: Evaluate the first page to see if it's a quiz
    EvaluateCurrentPageType();

    TryFireNextStepEvent(0);
}

void UTrainingStepPage::StartGM()
{
    AActor* GMActor = FindChapterGameManager();
    if (GMActor && GMActor->Implements<UStepSequencerInterface>())
    {
        IStepSequencerInterface::Execute_RunStepsOrder(GMActor);
    }
}

AActor* UTrainingStepPage::FindChapterGameManager()
{
    UGameInstance* GI = GetGameInstance();
    UChapterSubsystem* Subsystem = nullptr;
    if (GI) Subsystem = GI->GetSubsystem<UChapterSubsystem>();

    if (Subsystem)
    {
        TSubclassOf<AActor> GMClass = Subsystem->GetCurrentChapterGameManagerClass();
        if (GMClass) return UGameplayStatics::GetActorOfClass(this, GMClass);
    }
    return nullptr;
}

void UTrainingStepPage::EvaluateCurrentPageType()
{
    
}

void UTrainingStepPage::ForceQuizStateFromBlueprint(bool bIsQuiz, int32 CorrectAnswerIndex)
{
    bIsCurrentPageAQuiz = bIsQuiz;
    CurrentQuizCorrectAnswer = CorrectAnswerIndex;

    if (StepProgressText)
    {
        if (bIsQuiz) StepProgressText->SetVisibility(ESlateVisibility::Collapsed);
        else StepProgressText->SetVisibility(ESlateVisibility::Visible);
    }

    if (bIsQuiz)
    {
        bIsAwaitingQuizConfirm = (CurrentViewingIndex == MaxUnlockedIndex);

        if (PageProgressMemory.IsValidIndex(CurrentViewingIndex))
        {
            int32 SavedAnswer = PageProgressMemory[CurrentViewingIndex];
            if (SavedAnswer != 0) SelectedQuizAnswerIndex = SavedAnswer - 1;
            else SelectedQuizAnswerIndex = -1;
        }
        else SelectedQuizAnswerIndex = -1;

        OnQuizStateEntered(true);
    }
    else
    {
        bIsAwaitingQuizConfirm = false;
        OnQuizStateEntered(false);
    }

    UpdateNextButtonState();
    OnUpdateNavigationButtons();
}

void UTrainingStepPage::SelectQuizAnswer(int32 AnswerIndex)
{
    if (bIsAwaitingQuizConfirm)
    {
        SelectedQuizAnswerIndex = AnswerIndex;

        if (PageProgressMemory.IsValidIndex(CurrentViewingIndex))
        {
            PageProgressMemory[CurrentViewingIndex] = AnswerIndex + 1;
        }

        for (UQuizAnswerRow* Row : SpawnedQuizRows)
        {
            if (IsValid(Row))
            {
                if (Row->AnswerIndex == AnswerIndex) Row->SetRowState(EQuizAnswerState::Selected);
                // ---> NEW: Set non-selected rows to Unselected instead of Idle <---
                else Row->SetRowState(EQuizAnswerState::Unselected);
            }
        }

        OnUpdateNavigationButtons();
    }
}

void UTrainingStepPage::ConfirmQuizAnswer()
{
    if (SelectedQuizAnswerIndex == -1) return;
    if (!PageProgressMemory.IsValidIndex(CurrentViewingIndex)) return;

    bIsAwaitingQuizConfirm = false;
    bool bCorrect = (SelectedQuizAnswerIndex == CurrentQuizCorrectAnswer);

    if (bCorrect && QuizCorrectSound) UGameplayStatics::PlaySound2D(this, QuizCorrectSound);
    else if (!bCorrect && QuizWrongSound) UGameplayStatics::PlaySound2D(this, QuizWrongSound);

    // ---> THE VERSION THAT WORKED PERFECTLY <---
    int32 StepNum = CurrentViewingIndex + 1;

    if (SessionSteps.IsValidIndex(CurrentViewingIndex))
    {
        StepNum = SessionSteps[CurrentViewingIndex].DisplayNumber;
    }

    int32 OptionNum = SelectedQuizAnswerIndex + 1;

    // Fire the Blueprint Event!
    OnQuizAnswerSubmitted(StepNum, OptionNum);

    // --- VISUAL EVALUATION LOOP ---
    for (UQuizAnswerRow* Row : SpawnedQuizRows)
    {
        if (IsValid(Row))
        {
            if (Row->AnswerIndex == SelectedQuizAnswerIndex)
            {
                if (bCorrect) Row->SetRowState(EQuizAnswerState::Correct);
                else Row->SetRowState(EQuizAnswerState::Wrong);
            }
            else Row->SetRowState(EQuizAnswerState::Unselected);

            Row->SetButtonEnabled(false);
        }
    }

    if (CurrentViewingIndex == MaxUnlockedIndex)
    {
        MaxUnlockedIndex++;
        OnMasterStepCompleted(CurrentViewingIndex);

        UGameInstance* GI = GetGameInstance();
        if (GI)
        {
            UChapterSubsystem* Subsystem = GI->GetSubsystem<UChapterSubsystem>();
            if (Subsystem) Subsystem->UpdateChapterProgress(MaxUnlockedIndex);
        }

        if (MaxUnlockedIndex == TotalStepsCount && !bSessionFinished)
        {
            bSessionFinished = true;
            if (GetWorld()) FinalSessionDuration = GetWorld()->GetTimeSeconds() - SessionStartTime;
        }
    }

    UpdateNextButtonState();
    OnUpdateNavigationButtons();
}

void UTrainingStepPage::RestoreQuizVisualStates()
{
    if (!PageProgressMemory.IsValidIndex(CurrentViewingIndex)) return;

    int32 SavedMemory = PageProgressMemory[CurrentViewingIndex];

    // If memory is 0, the page is fresh. EVERYTHING is Idle.
    if (SavedMemory == 0)
    {
        for (UQuizAnswerRow* Row : SpawnedQuizRows)
        {
            if (IsValid(Row))
            {
                Row->SetRowState(EQuizAnswerState::Idle);
                Row->SetButtonEnabled(true);
            }
        }
        return;
    }

    int32 SavedAnswerIndex = SavedMemory - 1;
    bool bAlreadyConfirmed = (CurrentViewingIndex < MaxUnlockedIndex);
    bool bCorrect = (SavedAnswerIndex == CurrentQuizCorrectAnswer);

    for (UQuizAnswerRow* Row : SpawnedQuizRows)
    {
        if (IsValid(Row))
        {
            if (Row->AnswerIndex == SavedAnswerIndex)
            {
                if (bAlreadyConfirmed)
                {
                    if (bCorrect) Row->SetRowState(EQuizAnswerState::Correct);
                    else Row->SetRowState(EQuizAnswerState::Wrong);
                }
                else
                {
                    Row->SetRowState(EQuizAnswerState::Selected);
                }
            }
            else
            {
                // ---> NEW: Restore the Unselected state for rows they didn't pick <---
                Row->SetRowState(EQuizAnswerState::Unselected);
            }

            Row->SetButtonEnabled(!bAlreadyConfirmed);
        }
    }
}


// --- TEXT & BUTTON STATE UPDATES ---

void UTrainingStepPage::UpdateNextButtonState()
{
    if (!NextButtonText) return;

    // NEW: Hijack the button text if waiting for an answer
    if (bIsAwaitingQuizConfirm)
    {
        NextButtonText->SetText(FText::FromString("Confirm"));
        return;
    }

    if (CurrentViewingIndex < TotalStepsCount)
    {
        NextButtonText->SetText(FText::FromString("Next"));
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UChapterSubsystem* Subsystem = nullptr;
    if (GI) Subsystem = GI->GetSubsystem<UChapterSubsystem>();

    if (Subsystem)
    {
        int32 NextUnplayed = Subsystem->GetNextUnplayedChapterIndex();
        if (NextUnplayed != -1)
        {
            NextButtonText->SetText(FText::FromString("Next Chapter"));
            return;
        }
    }

    NextButtonText->SetText(FText::FromString("Completed"));
}

bool UTrainingStepPage::IsNextButtonEnabled() const
{
    // NEW: If it's a quiz, only enable the button if they picked an answer
    if (bIsAwaitingQuizConfirm)
    {
        return SelectedQuizAnswerIndex != -1;
    }

    if (CurrentViewingIndex >= TotalStepsCount) return true;
    return (CurrentViewingIndex < TotalStepsCount) && (CurrentViewingIndex < MaxUnlockedIndex);
}

// --- NAVIGATION ---

void UTrainingStepPage::GoNextPage()
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("1. GoNextPage Clicked! Is Awaiting Confirm?: %s"), bIsAwaitingQuizConfirm ? TEXT("TRUE") : TEXT("FALSE")));


    if (bIsAwaitingQuizConfirm)
    {
        ConfirmQuizAnswer();
        return;
    }

    if (CurrentViewingIndex >= TotalStepsCount)
    {
        UGameInstance* GI = GetGameInstance();
        UChapterSubsystem* Subsystem = nullptr;
        if (GI) Subsystem = GI->GetSubsystem<UChapterSubsystem>();

        if (Subsystem)
        {
            int32 NextIndex = Subsystem->GetNextUnplayedChapterIndex();
            if (NextIndex != -1)
            {
                FName LevelName = Subsystem->SelectChapter(NextIndex);
                if (LevelName != NAME_None) UGameplayStatics::OpenLevel(this, LevelName);
            }
        }
        return;
    }

    if (IsNextButtonEnabled())
    {
        CurrentViewingIndex++;
        if (CurrentViewingIndex > FurthestVisitedIndex) FurthestVisitedIndex = CurrentViewingIndex;

        OnPageChanged(CurrentViewingIndex);
        EvaluateCurrentPageType(); // Check if the new page is a quiz
        RefreshList();

        if (CurrentViewingIndex == TotalStepsCount)
        {
            UGameInstance* GI = GetGameInstance();
            UChapterSubsystem* Subsystem = nullptr;
            if (GI) Subsystem = GI->GetSubsystem<UChapterSubsystem>();
            if (Subsystem) Subsystem->MarkCurrentChapterComplete();

            OnChapterCompleted();
        }
        else
        {
            TryFireNextStepEvent(CurrentViewingIndex);
        }

        UpdateNextButtonState();
        OnUpdateNavigationButtons();
    }
}

void UTrainingStepPage::GoBackPage()
{
    if (IsBackButtonEnabled())
    {
        CurrentViewingIndex--;
        OnPageChanged(CurrentViewingIndex);
        EvaluateCurrentPageType(); // Check if the old page was a quiz
        RefreshList();
        UpdateNextButtonState();
        OnUpdateNavigationButtons();
    }
}

void UTrainingStepPage::JumpToCurrentPage()
{
    if (CurrentViewingIndex != FurthestVisitedIndex)
    {
        CurrentViewingIndex = FurthestVisitedIndex;
        OnPageChanged(CurrentViewingIndex);
        EvaluateCurrentPageType();
        RefreshList();
        UpdateNextButtonState();
        OnUpdateNavigationButtons();
        if (CurrentViewingIndex < TotalStepsCount) TryFireNextStepEvent(CurrentViewingIndex);
        else if (CurrentViewingIndex == TotalStepsCount) OnChapterCompleted();
    }
}

// --- LOGIC ---

void UTrainingStepPage::SetTotalSessionSteps(int32 NewTotalSteps)
{
    TotalStepsCount = NewTotalSteps;
    PageProgressMemory.Init(0, NewTotalSteps);
    StepTriggerHistory.Init(false, NewTotalSteps);

    FurthestVisitedIndex = 0;
    CurrentViewingIndex = 0;
    MaxUnlockedIndex = 0;
    SessionStartTime = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
    bSessionFinished = false;

    UpdateNextButtonState();
    OnUpdateNavigationButtons();
}

void UTrainingStepPage::AdvanceProgress()
{
    if (!DynamicListContainer) return;

    // NEW: If Game Manager accidentally tries to advance a Quiz step, block it.
    // Quizzes are strictly advanced by the user clicking Confirm.
    if (SessionSteps.IsValidIndex(CurrentViewingIndex) && SessionSteps[CurrentViewingIndex].StepType == EMasterStepType::Quiz)
    {
        return;
    }

    if (CurrentViewingIndex != MaxUnlockedIndex && MaxUnlockedIndex < TotalStepsCount)
    {
        CurrentViewingIndex = MaxUnlockedIndex;
        if (CurrentViewingIndex > FurthestVisitedIndex) FurthestVisitedIndex = CurrentViewingIndex;

        OnPageChanged(CurrentViewingIndex);
        EvaluateCurrentPageType();
        TryFireNextStepEvent(CurrentViewingIndex);

        UpdateNextButtonState();
        OnUpdateNavigationButtons();
    }

    if (CurrentViewingIndex >= TotalStepsCount) return;
    if (!PageProgressMemory.IsValidIndex(CurrentViewingIndex)) return;

    PageProgressMemory[CurrentViewingIndex]++;
    RefreshList();

    if (StepAdvanceSound) UGameplayStatics::PlaySound2D(this, StepAdvanceSound);

    ScrollToActiveSubStep();

    int32 CurrentProgress = PageProgressMemory[CurrentViewingIndex];
    int32 TotalSubSteps = DynamicListContainer->GetChildrenCount();

    if (CurrentProgress >= TotalSubSteps)
    {
        int32 FinishedStepIndex = CurrentViewingIndex;
        if (MaxUnlockedIndex == CurrentViewingIndex) MaxUnlockedIndex++;

        OnMasterStepCompleted(FinishedStepIndex);

        UGameInstance* GI = GetGameInstance();
        UChapterSubsystem* Subsystem = nullptr;
        if (GI) Subsystem = GI->GetSubsystem<UChapterSubsystem>();
        if (Subsystem) Subsystem->UpdateChapterProgress(MaxUnlockedIndex);

        if (MaxUnlockedIndex == TotalStepsCount && !bSessionFinished)
        {
            bSessionFinished = true;
            if (GetWorld()) FinalSessionDuration = GetWorld()->GetTimeSeconds() - SessionStartTime;
        }

        UpdateNextButtonState();
        OnUpdateNavigationButtons();
    }
}

void UTrainingStepPage::ScrollToActiveSubStep()
{
    if (!MainScrollBox || !DynamicListContainer) return;
    int32 ActiveSubIndex = 0;
    if (PageProgressMemory.IsValidIndex(CurrentViewingIndex)) ActiveSubIndex = PageProgressMemory[CurrentViewingIndex];
    UWidget* TargetWidget = DynamicListContainer->GetChildAt(ActiveSubIndex);
    if (TargetWidget) MainScrollBox->ScrollWidgetIntoView(TargetWidget, true, EDescendantScrollDestination::IntoView, 0);
}

void UTrainingStepPage::ResetAllProgress()
{
    PageProgressMemory.Init(0, TotalStepsCount);
    StepTriggerHistory.Init(false, TotalStepsCount);

    CurrentViewingIndex = 0;
    MaxUnlockedIndex = 0;
    FurthestVisitedIndex = 0;
    SessionStartTime = (GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
    bSessionFinished = false;

    RefreshList();
    UpdateProgressText();
    OnPageChanged(0);
    EvaluateCurrentPageType();
    UpdateNextButtonState();
}

void UTrainingStepPage::RefreshList()
{
    if (CurrentViewingIndex >= TotalStepsCount) return;
    if (!DynamicListContainer) return;
    if (!PageProgressMemory.IsValidIndex(CurrentViewingIndex)) return;

    // ---> THE SAFE PROTECTION LINE <---
    if (SessionSteps.IsValidIndex(CurrentViewingIndex) && SessionSteps[CurrentViewingIndex].StepType == EMasterStepType::Quiz) return;

    int32 SavedProgress = PageProgressMemory[CurrentViewingIndex];
    int32 ChildCount = DynamicListContainer->GetChildrenCount();

    for (int32 i = 0; i < ChildCount; i++)
    {
        UWidget* ChildWidget = DynamicListContainer->GetChildAt(i);
        UTrainingSubStepRow* RowParams = Cast<UTrainingSubStepRow>(ChildWidget);

        if (RowParams)
        {
            if (i < SavedProgress) RowParams->SetState(ESubStepState::Completed);
            else if (i == SavedProgress) RowParams->SetState(SavedProgress >= ChildCount ? ESubStepState::Completed : ESubStepState::Active);
            else RowParams->SetState(ESubStepState::Idle);
        }
    }
    UpdateProgressText();
}

// --- GETTERS & HELPERS ---

void UTrainingStepPage::TryFireNextStepEvent(int32 StepIndex)
{
    if (StepTriggerHistory.IsValidIndex(StepIndex)) {
        if (!StepTriggerHistory[StepIndex]) {
            StepTriggerHistory[StepIndex] = true;
            OnNextStepTrigger(StepIndex);
            StartGM();
        }
    }
}

void UTrainingStepPage::UpdateProgressText()
{
    if (StepProgressText) StepProgressText->SetText(FText::FromString(GetStepProgressText()));
}

bool UTrainingStepPage::IsOnResultPage() const { return CurrentViewingIndex >= TotalStepsCount; }

FString UTrainingStepPage::GetStepProgressText() const {
    if (CurrentViewingIndex >= TotalStepsCount) return TEXT("");
    if (!DynamicListContainer || !PageProgressMemory.IsValidIndex(CurrentViewingIndex)) return TEXT("(0/0)");
    int32 Total = DynamicListContainer->GetChildrenCount();
    int32 Cur = PageProgressMemory[CurrentViewingIndex];
    if (Cur > Total) Cur = Total;
    return FString::Printf(TEXT("(%d/%d)"), Cur, Total);
}

FString UTrainingStepPage::GetMasterProgressText() const {
    if (TotalStepsCount == 0) return TEXT("(0/0)");
    int32 Comp = MaxUnlockedIndex;
    if (Comp > TotalStepsCount) Comp = TotalStepsCount;
    return FString::Printf(TEXT("(%d/%d)"), Comp, TotalStepsCount);
}

FString UTrainingStepPage::GetSessionTimeText() const {
    float Dur = bSessionFinished ? FinalSessionDuration : (GetWorld() ? GetWorld()->GetTimeSeconds() - SessionStartTime : 0.0f);
    int32 T = FMath::FloorToInt(Dur);
    return FString::Printf(TEXT("%d m %d sec"), T / 60, T % 60);
}

bool UTrainingStepPage::IsBackButtonEnabled() const { return CurrentViewingIndex > 0; }
bool UTrainingStepPage::IsFlipButtonVisible() const { return CurrentViewingIndex < FurthestVisitedIndex; }

void UTrainingStepPage::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bIsTextFadingIn)
    {
        // Read current opacity safely from whichever text block exists
        float CurrentOpacity = 0.0f;
        if (MainStepText) CurrentOpacity = MainStepText->GetRenderOpacity();
        else if (StepProgressText) CurrentOpacity = StepProgressText->GetRenderOpacity();

        // Calculate the smooth curve
        float NewOpacity = FMath::FInterpTo(CurrentOpacity, 1.0f, InDeltaTime, TextFadeSpeed);

        // Apply the fade to BOTH text blocks simultaneously!
        if (MainStepText) MainStepText->SetRenderOpacity(NewOpacity);
        if (StepProgressText) StepProgressText->SetRenderOpacity(NewOpacity);

        // Once it's fully visible, lock it and turn off the math
        if (NewOpacity >= 0.99f)
        {
            if (MainStepText) MainStepText->SetRenderOpacity(1.0f);
            if (StepProgressText) StepProgressText->SetRenderOpacity(1.0f);
            bIsTextFadingIn = false;
        }
    }
}

void UTrainingStepPage::TriggerMainStepFade()
{
    // Instantly hide BOTH text blocks so the tick can smoothly fade them back in
    if (MainStepText) MainStepText->SetRenderOpacity(0.0f);
    if (StepProgressText) StepProgressText->SetRenderOpacity(0.0f);

    bIsTextFadingIn = true;
}

bool UTrainingStepPage::GetQuizAnswerForAPI(int32& OutStepNumber, int32& OutSelectedOption)
{
    OutStepNumber = 0;
    OutSelectedOption = 0;

    // ---> NEW: Use our reliable tracker! <---
    if (bIsCurrentPageAQuiz)
    {
        // Safely grab the step number
        if (SessionSteps.IsValidIndex(CurrentViewingIndex))
        {
            OutStepNumber = SessionSteps[CurrentViewingIndex].DisplayNumber;
        }

        // Grab the Option
        if (SelectedQuizAnswerIndex != -1)
        {
            OutSelectedOption = SelectedQuizAnswerIndex + 1;
        }
        else if (PageProgressMemory.IsValidIndex(CurrentViewingIndex))
        {
            OutSelectedOption = PageProgressMemory[CurrentViewingIndex];
        }

        return true;
    }

    return false; 
}