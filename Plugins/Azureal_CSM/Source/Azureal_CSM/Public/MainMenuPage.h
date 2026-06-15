

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "ChapterBundle.h"
#include "ChapterSelectRow.h"
#include "MainMenuPage.generated.h"

UCLASS()
class AZUREAL_CSM_API UMainMenuPage : public UUserWidget
{
    GENERATED_BODY()

public:
    // --- BINDING ---
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // --- START SCREEN LOGIC ---
    UFUNCTION(BlueprintCallable, Category = "Menu Logic")
    void OnStartButtonClicked();

    // --- MODULE RESET ---
    UFUNCTION(BlueprintCallable, Category = "Menu Logic")
    void RestartWholeModule();

    // --- CURRENT CHAPTER RESTART LOGIC ---
    UFUNCTION(BlueprintCallable, Category = "Menu Logic")
    void LoadCurrentChapter();

    // --- RESTART LOGIC (Selected Chapter) ---
    UFUNCTION()
    void OnRestartConfirmed();

    UFUNCTION()
    void OnRestartCancelled();

    // --- VISIBILITY & STATE ---
    UFUNCTION(BlueprintPure, Category = "Menu Logic")
    bool IsStartScreenActive() const;

    UFUNCTION(BlueprintCallable, Category = "Menu Logic")
    void UpdateVisibilityByChapter();

    // --- VISUAL UPDATE ---
    UFUNCTION(BlueprintCallable, Category = "Menu Logic")
    void RefreshChapterRows();

    // --- GENERATION ---
    UFUNCTION(BlueprintCallable, Category = "Menu Logic")
    void GenerateChapterList(UChapterBundle* DataAsset);

    // --- NAVIGATION ---
    UFUNCTION(BlueprintCallable, Category = "Menu Logic")
    void ProceedToSelectedChapter();

    UFUNCTION(BlueprintCallable, Category = "Menu Logic")
    void ResetSelection();

    UFUNCTION(BlueprintPure, Category = "Menu Logic")
    bool IsProceedButtonEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Menu Logic")
    int32 GetSelectedChapterIndex() const { return SelectedChapterIndex; }

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    int32 CurrentProgressionIndex = 0;

    // --- TEXT HELPERS ---
    UFUNCTION(BlueprintPure, Category = "Menu Logic")
    FText GetModuleCompletionText() const;

    UFUNCTION(BlueprintPure, Category = "Menu Logic")
    FText GetCurrentChapterRestartText() const;

    // --- NEW: MODULE STATUS CHECKER ---
    UFUNCTION(BlueprintPure, Category = "Menu Logic")
    bool IsAllChaptersCompleted() const;

protected:
    // --- WIDGET BINDINGS (MUST HAVE CATEGORY) ---

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UWidget* StartPanel;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UWidget* StepPageContainer;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UWidget* SelectChapterPanel;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UPanelWidget* ChapterListContainer;

    // --- RESTART PANEL WIDGETS ---
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UWidget* RestartChapterCompletedPanel;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UTextBlock* RestartMessageText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UButton* Btn_ConfirmRestart;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UButton* Btn_CancelRestart;

    UPROPERTY(EditAnywhere, Category = "Setup")
    TSubclassOf<UChapterSelectRow> ChapterRowClass;

    UPROPERTY()
    TArray<UChapterSelectRow*> SpawnedRows;

    UPROPERTY()
    UChapterBundle* SavedDataAsset;

    int32 SelectedChapterIndex = -1;

private:
    UFUNCTION()
    void HandleRowClicked(int32 Index);
};