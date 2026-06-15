

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChapterBundle.h"
#include "ChapterSubsystem.generated.h"

// --- DECLARE EVENT ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChapterDataUpdated);

class UTrainingCurriculum;

UCLASS()
class AZUREAL_CSM_API UChapterSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // --- EVENT DISPATCHER ---
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnChapterDataUpdated OnChapterDataUpdated;

    // --- SESSION STATE ---
    UFUNCTION(BlueprintPure, Category = "Session State")
    bool HasPassedStartScreen() const { return bHasPassedStartScreen; }

    UFUNCTION(BlueprintCallable, Category = "Session State")
    void SetHasPassedStartScreen(bool bValue) { bHasPassedStartScreen = bValue; }

    // --- SETUP ---
    UFUNCTION(BlueprintCallable, Category = "Chapter System")
    void InitializeChapters(UChapterBundle* MasterList);

    // --- NAVIGATION ---
    UFUNCTION(BlueprintCallable, Category = "Chapter System")
    FName SelectChapter(int32 Index);

    UFUNCTION(BlueprintPure, Category = "Chapter System")
    int32 GetCurrentChapterIndex() const;

    UFUNCTION(BlueprintPure, Category = "Chapter System")
    int32 GetNextUnplayedChapterIndex();

    // --- COMPLETION & PROGRESS ---
    UFUNCTION(BlueprintCallable, Category = "Chapter System")
    void MarkCurrentChapterComplete();

    UFUNCTION(BlueprintCallable, Category = "Chapter System")
    void UpdateChapterProgress(int32 StepIndex);

    /** Completely wipes progress for a specific chapter. */
    UFUNCTION(BlueprintCallable, Category = "Chapter System")
    void ResetChapterProgress(int32 Index);

    /** Wipes ALL progress and resets session. */
    UFUNCTION(BlueprintCallable, Category = "Chapter System")
    void ResetAllModuleProgress();

    UFUNCTION(BlueprintPure, Category = "Chapter System")
    bool IsChapterComplete(int32 Index) const;

    /** Returns TRUE if every single chapter in the bundle has been completed. */
    UFUNCTION(BlueprintPure, Category = "Chapter System")
    bool IsModuleFullyComplete() const;

    UFUNCTION(BlueprintPure, Category = "Chapter System")
    void GetChapterProgress(int32 Index, int32& OutCurrent, int32& OutMax) const;

    // --- DATA HELPERS ---
    UFUNCTION(BlueprintPure, Category = "Chapter System")
    UTrainingCurriculum* GetCurrentStepData() const;

    UFUNCTION(BlueprintPure, Category = "Chapter System")
    UTrainingCurriculum* GetStepDataForIndex(int32 Index) const;

    // --- NEW: MANAGER HELPER ---
    UFUNCTION(BlueprintPure, Category = "Chapter System")
    TSubclassOf<AActor> GetCurrentChapterGameManagerClass() const;

    // --- INFO HELPERS ---
    UFUNCTION(BlueprintPure, Category = "Chapter System")
    void GetCurrentChapterInfo(int32& OutChapterNumber, FText& OutChapterTitle) const;

    // --- STATISTICS ---
    UFUNCTION(BlueprintPure, Category = "Chapter System")
    int32 GetTotalChapterCount() const;

    UFUNCTION(BlueprintPure, Category = "Chapter System")
    int32 GetTotalMasterStepCount() const;

    /** Returns current progress for the whole module (e.g. 3 out of 5 chapters completed). */
    UFUNCTION(BlueprintPure, Category = "Chapter System")
    void GetModuleCompletionStatus(int32& OutCompletedCount, int32& OutTotalCount) const;

private:
    UPROPERTY()
    UChapterBundle* ActiveBundle;

    int32 CurrentIndex = -1;
    TSet<int32> CompletedChapterIndexes;
    TMap<int32, int32> ChapterStepProgress;
    bool bHasPassedStartScreen = false;
};