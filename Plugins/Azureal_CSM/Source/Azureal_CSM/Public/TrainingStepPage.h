

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h" 
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "TrainingTypes.h"
#include "StepSequencerInterface.h"
#include "TrainingStepPage.generated.h"

class USoundBase;

UCLASS()
class AZUREAL_CSM_API UTrainingStepPage : public UUserWidget
{
    GENERATED_BODY()

public:
    // --- SETUP ---
    UFUNCTION(BlueprintPure, Category = "Quiz Logic")
    bool GetQuizAnswerForAPI(int32& OutStepNumber, int32& OutSelectedOption);


    UFUNCTION(BlueprintCallable, Category = "Quiz Logic")
    void RestoreQuizVisualStates();

    UFUNCTION(BlueprintCallable, Category = "Training Setup")
    void InitializePageData();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* StepAdvanceSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* QuizCorrectSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* QuizWrongSound;

    UFUNCTION(BlueprintCallable, Category = "Step Logic")
    void StartGM();

    UPROPERTY(BlueprintReadOnly, Category = "Training Data")
    TArray<FRuntimeStep> SessionSteps;

    // --- MAIN LOGIC ---
    UFUNCTION(BlueprintCallable, Category = "Training Logic")
    void AdvanceProgress();

    UFUNCTION(BlueprintCallable, Category = "Training Logic")
    void RefreshList();

    UFUNCTION(BlueprintCallable, Category = "Training Logic")
    void ResetAllProgress();

    UFUNCTION(BlueprintCallable, Category = "Training Setup")
    void SetTotalSessionSteps(int32 NewTotalSteps);

    // --- NAVIGATION ---
    UFUNCTION(BlueprintCallable, Category = "Training Navigation")
    void GoNextPage();

    UFUNCTION(BlueprintCallable, Category = "Training Navigation")
    void GoBackPage();

    UFUNCTION(BlueprintCallable, Category = "Training Navigation")
    void JumpToCurrentPage();

    // --- QUIZ LOGIC ---
    UPROPERTY(BlueprintReadOnly, Category = "Quiz State")
    bool bIsAwaitingQuizConfirm = false;

    UPROPERTY(BlueprintReadOnly, Category = "Quiz State")
    int32 SelectedQuizAnswerIndex = -1;

  
    UPROPERTY(BlueprintReadOnly, Category = "Quiz State")
    bool bIsCurrentPageAQuiz = false;

    UFUNCTION(BlueprintCallable, Category = "Quiz Logic")
    void SelectQuizAnswer(int32 AnswerIndex);

    UFUNCTION(BlueprintCallable, Category = "Quiz Logic")
    void ConfirmQuizAnswer();

    UFUNCTION(BlueprintCallable, Category = "Quiz Logic")
    void ForceQuizStateFromBlueprint(bool bIsQuiz, int32 CorrectAnswerIndex);

    UPROPERTY(BlueprintReadOnly, Category = "Quiz State")
    int32 CurrentQuizCorrectAnswer = -1;

    // --- UI ANIMATION (NEW) ---
    // Call this from Blueprint right after you SetText!
    UFUNCTION(BlueprintCallable, Category = "Training UI")
    void TriggerMainStepFade();

    // --- DATA GETTERS ---
    UFUNCTION(BlueprintPure, Category = "Training Results")
    FString GetSessionTimeText() const;

    UFUNCTION(BlueprintPure, Category = "Training Results")
    FString GetMasterProgressText() const;

    UFUNCTION(BlueprintPure, Category = "Training Results")
    FString GetStepProgressText() const;

    UFUNCTION(BlueprintPure, Category = "Training Navigation")
    bool IsOnResultPage() const;

    UFUNCTION(BlueprintPure, Category = "Training Navigation")
    bool IsNextButtonEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Training Navigation")
    bool IsBackButtonEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Training Navigation")
    bool IsFlipButtonVisible() const;

protected:
    // --- C++ ANIMATION ENGINE (NEW) ---
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // --- WIDGET BINDINGS ---
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UScrollBox* MainScrollBox;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UPanelWidget* DynamicListContainer;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UTextBlock* StepProgressText;

    // --- NEW: Bind the Main Step Text ---
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Components")
    UTextBlock* MainStepText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UTextBlock* NextButtonText;

    // The panel where C++ will automatically spawn the Quiz Buttons
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Components")
    UPanelWidget* QuizContainerBox;

    // The class to spawn (You will select WBP_QuizAnswerRow in the details panel)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
    TSubclassOf<class UQuizAnswerRow> QuizButtonClass;

    // Internal memory of the spawned rows (Exposed so Blueprint can register rows)
    UPROPERTY(BlueprintReadWrite, Category = "Quiz Internal")
    TArray<UQuizAnswerRow*> SpawnedQuizRows;

   
    UFUNCTION(BlueprintImplementableEvent, Category = "Quiz Events")
    void OnQuizAnswerSubmitted(int32 StepNumber, int32 SelectedOption);

    // The event that tells Blueprints to flip the Widget Switcher
    UFUNCTION(BlueprintImplementableEvent, Category = "Quiz Events")
    void OnQuizStateEntered(bool bIsQuiz);

    // --- INTERNAL DATA ---
    TArray<int32> PageProgressMemory;
    TArray<bool> StepTriggerHistory;

    int32 FurthestVisitedIndex = 0;
    int32 MaxUnlockedIndex = 0;
    int32 CurrentViewingIndex = 0;
    int32 TotalStepsCount = 0;

    float SessionStartTime = 0.0f;
    float FinalSessionDuration = 0.0f;
    bool bSessionFinished = false;

    
    bool bIsTextFadingIn = false;
    float TextFadeSpeed = 4.0f;

    // --- RETRY SYSTEM ---
    FTimerHandle InitRetryTimer;
    int32 InitRetryCount = 0;
    void TryInitializeData();

    // --- HELPERS ---
    void UpdateProgressText();
    void TryFireNextStepEvent(int32 StepIndex);
    void UpdateNextButtonState();
    void ScrollToActiveSubStep();
    AActor* FindChapterGameManager();
    void EvaluateCurrentPageType();

    // --- EVENTS ---
    UFUNCTION(BlueprintImplementableEvent, Category = "Training Events")
    void OnPageChanged(int32 NewPageIndex);

    UFUNCTION(BlueprintImplementableEvent, Category = "Training Events")
    void OnUpdateNavigationButtons();

    UFUNCTION(BlueprintImplementableEvent, Category = "Training Events")
    void OnMasterStepCompleted(int32 CompletedStepIndex);

    UFUNCTION(BlueprintImplementableEvent, Category = "Training Events")
    void OnChapterCompleted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Training Events")
    void OnNextStepTrigger(int32 NewMasterStepIndex);
};