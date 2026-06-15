

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Azr_ActionWidget.generated.h"

// Forward Declarations
class UButton;
class UTextBlock;
class UProgressBar;

// Determines the behavior and text of the Action UI
UENUM(BlueprintType)
enum class EAzr_ActionMode : uint8 {
	Teleport   UMETA(DisplayName = "Teleport"),
	Animation  UMETA(DisplayName = "Animation Sequence"),
	Custom     UMETA(DisplayName = "Custom Timer (Blueprint)")
};

// Tracks the current visual state of the widget
UENUM(BlueprintType)
enum class EAzr_ActionWidgetState : uint8 {
	Idle,       // Ready to be clicked ("START" or "TELEPORT")
	Processing, // Action is running, button is locked ("ACTION ON GOING...")
	Completed   // Action done ("Completed")
};

// --- EVENTS ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActionExecute);

/**
 * UAzr_ActionWidget
 * The C++ base for the AzurealXR Action UI.
 * Manages the Idle/Processing/Completed states based on the Action Mode.
 */
UCLASS(Abstract)
class AZUREALXR_API UAzr_ActionWidget : public UUserWidget {
	GENERATED_BODY()

public:
	// --- EVENTS FOR THE COMPONENT TO LISTEN TO ---
	UPROPERTY(BlueprintAssignable, Category = "Azureal|Action Events")
	FOnActionExecute OnExecuteClicked;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Action Events")
	FOnActionExecute OnCompletedClicked;

	// --- PUBLIC API FOR THE COMPONENT TO CALL ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Action API")
	void InitializeAction(EAzr_ActionMode InMode);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Action API")
	void SetActionProgress(float Progress);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Action API")
	void SetActionCompleted();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Action API")
	void SetHoveredByLeftHand(bool bIsHovering);

	UFUNCTION(BlueprintCallable, Category = "Azureal|Action API")
	void SetHoveredByRightHand(bool bIsHovering);

	// NEW: Allows the logic component to inject the paragraph text
	UFUNCTION(BlueprintCallable, Category = "Azureal|Action API")
	void SetActionDescription(const FText& NewText);

protected:
	virtual void NativeConstruct() override;

	// --- EXACT UMG BINDINGS ---
	UPROPERTY(BlueprintReadWrite, Category = "Azureal|UI", meta = (BindWidget))
	UButton* ActionButton;

	UPROPERTY(BlueprintReadWrite, Category = "Azureal|UI", meta = (BindWidget))
	UTextBlock* ActionText;

	UPROPERTY(BlueprintReadWrite, Category = "Azureal|UI", meta = (BindWidget))
	UProgressBar* ActionProgressBar;

	// NEW: The text block where the actual action description paragraph will go
	UPROPERTY(BlueprintReadWrite, Category = "Azureal|UI", meta = (BindWidget))
	UTextBlock* ActionDescriptionBlock;

private:
	UFUNCTION()
	void OnActionButtonClicked();

	UFUNCTION()
	void OnActionButtonHovered();

	UFUNCTION()
	void OnActionButtonUnhovered();

	void UpdateButtonVisuals();

	// --- STATE TRACKING ---
	EAzr_ActionMode CurrentMode;
	EAzr_ActionWidgetState CurrentState;

	bool bIsLeftHandHovering;
	bool bIsRightHandHovering;
};