

#include "ChapterSubsystem.h"
#include "ExplanationFlowLibrary.h" 
#include "TrainingCurriculum.h"     

// --- SETUP & NAVIGATION ---

void UChapterSubsystem::InitializeChapters(UChapterBundle* MasterList)
{
    if (MasterList)
    {
        ActiveBundle = MasterList;
        if (CurrentIndex == -1) CurrentIndex = 0;
    }
}

FName UChapterSubsystem::SelectChapter(int32 Index)
{
    if (ActiveBundle && ActiveBundle->AllChapters.IsValidIndex(Index))
    {
        CurrentIndex = Index;
        TSoftObjectPtr<UWorld> LevelPtr = ActiveBundle->AllChapters[Index].ChapterLevel;
        if (LevelPtr.IsNull()) return NAME_None;
        return FName(*LevelPtr.GetAssetName());
    }
    return NAME_None;
}

int32 UChapterSubsystem::GetCurrentChapterIndex() const
{
    return (CurrentIndex == -1) ? 0 : CurrentIndex;
}

int32 UChapterSubsystem::GetNextUnplayedChapterIndex()
{
    if (!ActiveBundle) return -1;
    for (int32 i = 0; i < ActiveBundle->AllChapters.Num(); i++)
    {
        if (!CompletedChapterIndexes.Contains(i)) return i;
    }
    return -1;
}

// --- COMPLETION & PROGRESS LOGIC ---

void UChapterSubsystem::MarkCurrentChapterComplete()
{
    int32 IndexToMark = (CurrentIndex == -1) ? 0 : CurrentIndex;
    CompletedChapterIndexes.Add(IndexToMark);

    if (OnChapterDataUpdated.IsBound()) OnChapterDataUpdated.Broadcast();
}

void UChapterSubsystem::UpdateChapterProgress(int32 StepIndex)
{
    int32 SafeIndex = (CurrentIndex == -1) ? 0 : CurrentIndex;
    bool bChanged = false;

    if (ChapterStepProgress.Contains(SafeIndex))
    {
        if (StepIndex > ChapterStepProgress[SafeIndex])
        {
            ChapterStepProgress[SafeIndex] = StepIndex;
            bChanged = true;
        }
    }
    else
    {
        ChapterStepProgress.Add(SafeIndex, StepIndex);
        bChanged = true;
    }

    if (bChanged && OnChapterDataUpdated.IsBound())
    {
        OnChapterDataUpdated.Broadcast();
    }
}

void UChapterSubsystem::ResetChapterProgress(int32 Index)
{
    bool bChanged = false;

    // 1. Remove from Completed List
    if (CompletedChapterIndexes.Contains(Index))
    {
        CompletedChapterIndexes.Remove(Index);
        bChanged = true;
    }

    // 2. Remove from Progress Map
    if (ChapterStepProgress.Contains(Index))
    {
        ChapterStepProgress.Remove(Index);
        bChanged = true;
    }

    if (bChanged && OnChapterDataUpdated.IsBound())
    {
        OnChapterDataUpdated.Broadcast();
    }
}

void UChapterSubsystem::ResetAllModuleProgress()
{
    CompletedChapterIndexes.Empty();
    ChapterStepProgress.Empty();
    bHasPassedStartScreen = false;

    if (OnChapterDataUpdated.IsBound())
    {
        OnChapterDataUpdated.Broadcast();
    }
}

bool UChapterSubsystem::IsChapterComplete(int32 Index) const
{
    return CompletedChapterIndexes.Contains(Index);
}

bool UChapterSubsystem::IsModuleFullyComplete() const
{
    if (!ActiveBundle) return false;
    int32 Total = ActiveBundle->AllChapters.Num();
    return (Total > 0 && CompletedChapterIndexes.Num() >= Total);
}

void UChapterSubsystem::GetChapterProgress(int32 Index, int32& OutCurrent, int32& OutMax) const
{
    OutCurrent = 0;
    OutMax = 0;
    if (!ActiveBundle || !ActiveBundle->AllChapters.IsValidIndex(Index)) return;

    if (ActiveBundle->AllChapters[Index].StepData)
    {
        bool bIncludeExplanations = UExplanationFlowLibrary::IsExplanationEnabled();
        OutMax = ActiveBundle->AllChapters[Index].StepData->CountFilteredSteps(bIncludeExplanations);
    }

    if (IsChapterComplete(Index)) OutCurrent = OutMax;
    else if (ChapterStepProgress.Contains(Index)) OutCurrent = ChapterStepProgress[Index];
}

// --- DATA HELPERS ---

UTrainingCurriculum* UChapterSubsystem::GetCurrentStepData() const
{
    int32 SafeIndex = (CurrentIndex == -1) ? 0 : CurrentIndex;
    if (ActiveBundle && ActiveBundle->AllChapters.IsValidIndex(SafeIndex))
        return ActiveBundle->AllChapters[SafeIndex].StepData;
    return nullptr;
}

UTrainingCurriculum* UChapterSubsystem::GetStepDataForIndex(int32 Index) const
{
    if (ActiveBundle && ActiveBundle->AllChapters.IsValidIndex(Index))
        return ActiveBundle->AllChapters[Index].StepData;
    return nullptr;
}

// --- NEW: MANAGER HELPER ---
TSubclassOf<AActor> UChapterSubsystem::GetCurrentChapterGameManagerClass() const
{
    int32 SafeIndex = (CurrentIndex == -1) ? 0 : CurrentIndex;
    if (ActiveBundle && ActiveBundle->AllChapters.IsValidIndex(SafeIndex))
    {
        return ActiveBundle->AllChapters[SafeIndex].ChapterGameManagerClass;
    }
    return nullptr;
}

// --- INFO & STATS ---

void UChapterSubsystem::GetCurrentChapterInfo(int32& OutChapterNumber, FText& OutChapterTitle) const
{
    int32 SafeIndex = (CurrentIndex == -1) ? 0 : CurrentIndex;
    OutChapterNumber = SafeIndex + 1;
    OutChapterTitle = FText::FromString("Unknown Chapter");

    if (ActiveBundle && ActiveBundle->AllChapters.IsValidIndex(SafeIndex))
        OutChapterTitle = ActiveBundle->AllChapters[SafeIndex].ChapterTitle;
}

int32 UChapterSubsystem::GetTotalChapterCount() const
{
    return ActiveBundle ? ActiveBundle->AllChapters.Num() : 0;
}

int32 UChapterSubsystem::GetTotalMasterStepCount() const
{
    if (!ActiveBundle) return 0;
    int32 TotalSteps = 0;
    bool bIncludeExplanations = UExplanationFlowLibrary::IsExplanationEnabled();
    for (const FChapterDef& Chapter : ActiveBundle->AllChapters)
    {
        if (Chapter.StepData) TotalSteps += Chapter.StepData->CountFilteredSteps(bIncludeExplanations);
    }
    return TotalSteps;
}

void UChapterSubsystem::GetModuleCompletionStatus(int32& OutCompletedCount, int32& OutTotalCount) const
{
    OutCompletedCount = CompletedChapterIndexes.Num();
    OutTotalCount = (ActiveBundle != nullptr) ? ActiveBundle->AllChapters.Num() : 0;
}