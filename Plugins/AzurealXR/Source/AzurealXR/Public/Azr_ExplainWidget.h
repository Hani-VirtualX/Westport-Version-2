

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Azr_ExplainWidget.generated.h"

// Forward Declarations
class UButton;
class UTextBlock;
class UProgressBar;

// Determines what text the button shows when audio is done
UENUM(BlueprintType)
enum class EAzr_ExplainStepType : uint8 {
	Single, // Normal Explain (Completes with "CONFIRM")
	Start,  // Explain+ (Completes with "CONTINUE")
	Middle, // Explain+ (Completes with "CONTINUE")
	End     // Explain+ (Completes with "CONFIRM")
};

// Tracks the current visual state of the widget
UENUM(BlueprintType)
enum class EAzr_ExplainWidgetState : uint8 {
	Idle,     // Waiting for player to click Play
	Playing,  // Audio is running, button is locked
	Completed // Audio done, waiting for player to click Continue/Confirm
};

// --- EVENTS ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExplainPlayAudio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExplainProceed);

/**
 * UAzr_ExplainWidget
 * The C++ base for the AzurealXR Explanation UI.
 * Manages the Play/Progress/Continue state machine.
 */
UCLASS(Abstract)
class AZUREALXR_API UAzr_ExplainWidget : public UUserWidget {
	GENERATED_BODY()

public:
	// --- EVENTS FOR THE COMPONENT TO LISTEN TO ---
	UPROPERTY(BlueprintAssignable, Category = "Azureal|Explain Events")
	FOnExplainPlayAudio OnPlayAudioClicked;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Explain Events")
	FOnExplainProceed OnProceedClicked;

	// --- PUBLIC API FOR THE COMPONENT TO CALL ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Explain API")
	void InitializeStep(EAzr_ExplainStepType InStepType);

	// NEW: Allows the logic component to inject the paragraph text
	UFUNCTION(BlueprintCallable, Category = "Azureal|Explain API")
	void SetExplainText(const FText& NewText);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Explain API")
	void SetAudioProgress(float Progress);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Explain API")
	void SetPlaybackCompleted();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Explain API")
	void SetHoveredByLeftHand(bool bIsHovering);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Explain API")
	void SetHoveredByRightHand(bool bIsHovering);

protected:
	virtual void NativeConstruct() override;

	// --- EXACT UMG BINDINGS ---
	UPROPERTY(BlueprintReadWrite, Category = "Azureal|UI", meta = (BindWidget))
	UButton* InteractionButton;

	UPROPERTY(BlueprintReadWrite, Category = "Azureal|UI", meta = (BindWidget))
	UTextBlock* ActionText;

	UPROPERTY(BlueprintReadWrite, Category = "Azureal|UI", meta = (BindWidget))
	UTextBlock* ExplainTextBlock;

	UPROPERTY(BlueprintReadWrite, Category = "Azureal|UI", meta = (BindWidget))
	UProgressBar* AudioProgressBar;

private:
	UFUNCTION()
	void OnInteractionButtonClicked();

	UFUNCTION()
	void OnInteractionButtonHovered();

	UFUNCTION()
	void OnInteractionButtonUnhovered();

	void UpdateButtonVisuals();

	// --- STATE TRACKING ---
	EAzr_ExplainStepType CurrentStepType;
	EAzr_ExplainWidgetState CurrentState;

	bool bIsLeftHandHovering;
	bool bIsRightHandHovering;
};