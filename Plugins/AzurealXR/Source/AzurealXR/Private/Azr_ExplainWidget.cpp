

#include "Azr_ExplainWidget.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h" 
#include "Engine/World.h" 

void UAzr_ExplainWidget::NativeConstruct() {
    Super::NativeConstruct();

    if (InteractionButton) {
        InteractionButton->OnClicked.AddDynamic(this, &UAzr_ExplainWidget::OnInteractionButtonClicked);
        InteractionButton->OnHovered.AddDynamic(this, &UAzr_ExplainWidget::OnInteractionButtonHovered);
        InteractionButton->OnUnhovered.AddDynamic(this, &UAzr_ExplainWidget::OnInteractionButtonUnhovered);
    }

    

    CurrentState = EAzr_ExplainWidgetState::Idle;
    bIsLeftHandHovering = false;
    bIsRightHandHovering = false;
}

void UAzr_ExplainWidget::InitializeStep(EAzr_ExplainStepType InStepType) {
    CurrentStepType = InStepType;
    CurrentState = EAzr_ExplainWidgetState::Idle;

    if (AudioProgressBar) {
        AudioProgressBar->SetPercent(0.0f);
    }

    UpdateButtonVisuals();
}

// --- NEW: INJECTS THE DEV TEXT INTO THE UI ---
void UAzr_ExplainWidget::SetExplainText(const FText& NewText) {
    if (ExplainTextBlock) {
        ExplainTextBlock->SetText(NewText);
    }
}

void UAzr_ExplainWidget::OnInteractionButtonClicked() {
    if (APlayerController* PC = Cast<APlayerController>(GetOwningPlayer())) {
        if (bIsLeftHandHovering) PC->SetHapticsByValue(1.0f, 1.0f, EControllerHand::Left);
        if (bIsRightHandHovering) PC->SetHapticsByValue(1.0f, 1.0f, EControllerHand::Right);
    }

    if (CurrentState == EAzr_ExplainWidgetState::Idle) {
        CurrentState = EAzr_ExplainWidgetState::Playing;
        UpdateButtonVisuals();
        OnPlayAudioClicked.Broadcast();
    }
    else if (CurrentState == EAzr_ExplainWidgetState::Completed) {
        OnProceedClicked.Broadcast();
    }
}

// --- NATIVE HOVER EVENTS (HAPTICS ONLY) ---
void UAzr_ExplainWidget::OnInteractionButtonHovered() {
    if (CurrentState == EAzr_ExplainWidgetState::Playing) return;

    if (APlayerController* PC = Cast<APlayerController>(GetOwningPlayer())) {
        if (bIsLeftHandHovering) PC->SetHapticsByValue(1.0f, 1.0f, EControllerHand::Left);
        if (bIsRightHandHovering) PC->SetHapticsByValue(1.0f, 1.0f, EControllerHand::Right);
    }
}

void UAzr_ExplainWidget::OnInteractionButtonUnhovered() {
    if (APlayerController* PC = Cast<APlayerController>(GetOwningPlayer())) {
        if (bIsLeftHandHovering) PC->SetHapticsByValue(0.5f, 0.5f, EControllerHand::Left);
        if (bIsRightHandHovering) PC->SetHapticsByValue(0.5f, 0.5f, EControllerHand::Right);
    }
}

void UAzr_ExplainWidget::SetHoveredByLeftHand(bool bIsHovering) {
    bIsLeftHandHovering = bIsHovering;

    if (APlayerController* PC = Cast<APlayerController>(GetOwningPlayer())) {
        if (bIsHovering) {
            PC->SetHapticsByValue(0.5f, 0.5f, EControllerHand::Left);
        }
        else {
            PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Left);
        }
    }
}

void UAzr_ExplainWidget::SetHoveredByRightHand(bool bIsHovering) {
    bIsRightHandHovering = bIsHovering;

    if (APlayerController* PC = Cast<APlayerController>(GetOwningPlayer())) {
        if (bIsHovering) {
            PC->SetHapticsByValue(0.5f, 0.5f, EControllerHand::Right);
        }
        else {
            PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Right);
        }
    }
}

void UAzr_ExplainWidget::SetAudioProgress(float Progress) {
    if (AudioProgressBar && CurrentState == EAzr_ExplainWidgetState::Playing) {
        float ClampedProgress = FMath::Clamp(Progress, 0.0f, 1.0f);

        if (ClampedProgress >= 1.0f) {
            SetPlaybackCompleted();
        }
        else {
            AudioProgressBar->SetPercent(ClampedProgress);
        }
    }
}

void UAzr_ExplainWidget::SetPlaybackCompleted() {
    if (CurrentState == EAzr_ExplainWidgetState::Playing) {
        CurrentState = EAzr_ExplainWidgetState::Completed;

        if (APlayerController* PC = Cast<APlayerController>(GetOwningPlayer())) {
            PC->SetHapticsByValue(1.0f, 1.0f, EControllerHand::Left);
            PC->SetHapticsByValue(1.0f, 1.0f, EControllerHand::Right);

            if (UWorld* World = GetWorld()) {
                FTimerHandle HapticTimerHandle;
                World->GetTimerManager().SetTimer(HapticTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]() {
                    if (APlayerController* InnerPC = Cast<APlayerController>(GetOwningPlayer())) {
                        InnerPC->SetHapticsByValue(bIsLeftHandHovering ? 0.5f : 0.0f, bIsLeftHandHovering ? 0.5f : 0.0f, EControllerHand::Left);
                        InnerPC->SetHapticsByValue(bIsRightHandHovering ? 0.5f : 0.0f, bIsRightHandHovering ? 0.5f : 0.0f, EControllerHand::Right);
                    }
                    }), 0.5f, false);
            }
        }

        if (AudioProgressBar) {
            AudioProgressBar->SetPercent(0.0f);
        }

        UpdateButtonVisuals();
    }
}

void UAzr_ExplainWidget::UpdateButtonVisuals() {
    if (!InteractionButton || !ActionText) return;

    switch (CurrentState) {
    case EAzr_ExplainWidgetState::Idle:
        InteractionButton->SetIsEnabled(true);
        ActionText->SetText(FText::FromString(TEXT("PLAY AUDIO")));
        break;

    case EAzr_ExplainWidgetState::Playing:
        InteractionButton->SetIsEnabled(false);
        ActionText->SetText(FText::FromString(TEXT("PLAYING AUDIO...")));
        break;

    case EAzr_ExplainWidgetState::Completed:
        InteractionButton->SetIsEnabled(true);

        if (CurrentStepType == EAzr_ExplainStepType::Single ||
            CurrentStepType == EAzr_ExplainStepType::End) {
            ActionText->SetText(FText::FromString(TEXT("CONFIRM")));
        }
        else {
            ActionText->SetText(FText::FromString(TEXT("CONTINUE")));
        }
        break;
    }
}