

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h" 
#include "ChapterSelectRow.generated.h"

// The 3 Distinct Navigation States (Interaction)
UENUM(BlueprintType)
enum class EChapterState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Selected    UMETA(DisplayName = "Selected"),
    YouAreHere  UMETA(DisplayName = "You Are Here")
};

// --- NEW: The 3 Distinct Completion States (Visual) ---
UENUM(BlueprintType)
enum class EChapterCompletionState : uint8
{
    Incomplete  UMETA(DisplayName = "Incomplete"),
    InProgress  UMETA(DisplayName = "In Progress"),
    Completed   UMETA(DisplayName = "Completed")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChapterRowClicked, int32, Index);

UCLASS()
class AZUREAL_CSM_API UChapterSelectRow : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnChapterRowClicked OnRowClicked;

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    int32 AssignedIndex;

    // --- SETUP ---
    UFUNCTION(BlueprintCallable, Category = "Setup")
    void SetupSpecificRow(int32 Index, FText Title, int32 CurrentSteps, int32 MaxSteps, bool bIsComplete, bool bIsCurrentLocation);

    // --- INTERACTION ---
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetRowState(EChapterState NewState);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void BroadcastClick();

protected:
    // --- WIDGET BINDINGS (MUST HAVE CATEGORY) ---

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UTextBlock* ChapterNumberText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UTextBlock* ChapterTitleText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UTextBlock* ProgressText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UImage* StatusIconImage;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UButton* ChapterButton;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UImage* CompletionStatusImage;

    // --- UPDATED EVENT ---
    UFUNCTION(BlueprintImplementableEvent, Category = "Visuals")
    void UpdateCompletionImage(EChapterCompletionState State);

    UFUNCTION(BlueprintImplementableEvent, Category = "Visuals")
    void OnStateChanged(EChapterState NewState);
};