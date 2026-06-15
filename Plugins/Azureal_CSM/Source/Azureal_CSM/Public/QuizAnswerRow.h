

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h" 
#include "QuizAnswerRow.generated.h"

class UTrainingStepPage;

UENUM(BlueprintType)
enum class EQuizAnswerState : uint8
{
    Idle        UMETA(DisplayName = "Idle (Default/Unclicked)"),
    Selected    UMETA(DisplayName = "Selected (Highlighted)"),
    Unselected  UMETA(DisplayName = "Unselected (Dimmed)"),
    Correct     UMETA(DisplayName = "Correct (Green Check)"),
    Wrong       UMETA(DisplayName = "Wrong (Red X)")
};

UCLASS()
class AZUREAL_CSM_API UQuizAnswerRow : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Quiz Logic")
    void SetButtonEnabled(bool bInEnabled);

    UPROPERTY(BlueprintReadOnly, Category = "Quiz Data")
    int32 AnswerIndex = -1;

    // Called by C++ to build the widget
    UFUNCTION(BlueprintCallable, Category = "Quiz Logic")
    void SetupRow(int32 InIndex, FText InAnswerText, UTrainingStepPage* InParent);

    // Called by C++ to change the color/icon
    UFUNCTION(BlueprintCallable, Category = "Quiz Logic")
    void SetRowState(EQuizAnswerState NewState);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // --- NEW: The C++ Native Tick function ---
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // --- BINDINGS ---
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UButton* AnswerButton;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Components")
    UTextBlock* AnswerText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Components")
    UImage* ResultIcon;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Components")
    UImage* ArrowTickIndicator;

    UFUNCTION(BlueprintImplementableEvent, Category = "Quiz Events")
    void OnStateChanged(EQuizAnswerState NewState);

private:

    UFUNCTION()
    void OnButtonPressed();

    UPROPERTY()
    UTrainingStepPage* ParentPage;

    EQuizAnswerState CurrentState = EQuizAnswerState::Idle;

    // --- NEW: Fade Math Variables ---
    bool bIsFadingIn = false;
    float FadeSpeed = 4.0f; // Adjust this number! Higher = Faster fade.
};