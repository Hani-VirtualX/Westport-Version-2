

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Azr_Types.h" 
#include "Azr_ActionWidget.h" // Needed for the enums
#include "Azr_Action.generated.h"

// Forward Declarations
class UWidgetComponent;
class UCableComponent;
class UStaticMeshComponent;
class UMaterialParameterCollection;
class UAzr_ActionWidget;
class UAzr_Pointer;
class UAnimSequence;

// --- STRUCTS ---
USTRUCT(BlueprintType)
struct FAzr_ActionAnimPayload
{
	GENERATED_BODY()

	// Use a Soft Object Pointer so the Eyedropper works perfectly in Components!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Payload")
	TSoftObjectPtr<AActor> TargetActor;

	// NEW: The exact name of the Skeletal Mesh Component (leave as "None" to just use the first one found)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Payload")
	FName TargetComponentName = NAME_None;

	// The animation to play on that Actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Payload")
	UAnimSequence* AnimToPlay = nullptr;
};

// --- EVENTS ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActionFinished);

/**
 * UAzr_Action
 * The logic manager for AzurealXR active executions (Teleport, Animations, Custom Timers).
 */
UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent, DisplayName = "Azr Action Logic"))
class AZUREALXR_API UAzr_Action : public UActorComponent
{
	GENERATED_BODY()

public:
	UAzr_Action();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// --- CORE SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings")
	int32 InteractID = 1;

	// Determines the behavior of this action when clicked
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Configuration")
	EAzr_ActionMode ActionMode = EAzr_ActionMode::Teleport;

	// --- 1. TELEPORT MODE ---
	// Target point in the level to teleport the player to (Use Eyedropper)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Teleport", meta = (EditCondition = "ActionMode == EAzr_ActionMode::Teleport", EditConditionHides))
	TSoftObjectPtr<AActor> TeleportTargetPoint;

	// --- 2. ANIMATION MODE ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Animation", meta = (EditCondition = "ActionMode == EAzr_ActionMode::Animation", EditConditionHides))
	TArray<FAzr_ActionAnimPayload> AnimPayloads;

	// --- 3. CUSTOM TIMER MODE ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Custom Timer", meta = (EditCondition = "ActionMode == EAzr_ActionMode::Custom", EditConditionHides))
	float CustomDuration = 3.0f;

	// NEW: The written explanation text to display on the UI
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Visuals", meta = (MultiLine = true))
	FText ActionDescription;

	// --- VISUAL SETUP ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Visuals")
	FName WidgetName = "ActionWidget";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Visuals")
	FName TargetMeshName = "TargetMesh";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Visuals")
	EAzr_HighlightMode HighlightMode = EAzr_HighlightMode::AllComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Visuals")
	float HighlightSpeed = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Visuals")
	FAzr_TetherConfig TetherSettings;

	// --- AUDIO ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Audio")
	USoundBase* SoundHighlightStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Settings|Audio")
	USoundBase* SoundHighlightEnd;

	// --- EVENTS ---
	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnActionFinished OnActionCompleted; // Fires automatically when the progress bar hits 100%

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnActionFinished OnStartButtonPressed; // Fires when the player clicks "Start/Teleport"

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnActionFinished OnCompletedButtonPressed; // Fires when the player clicks "Completed"

	// --- API ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableAction();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisableAction();

private:
	// --- INTERNAL COMPONENTS ---
	UPROPERTY()
	UStaticMeshComponent* StartAnchor;
	UPROPERTY()
	UStaticMeshComponent* EndAnchor;
	UPROPERTY()
	UCableComponent* TetherCable;
	UPROPERTY()
	UMaterialParameterCollection* HighlightMPC;
	UPROPERTY()
	UPrimitiveComponent* TargetMesh;

	UPROPERTY()
	UWidgetComponent* ActiveWidgetComp;

	UPROPERTY()
	UAzr_ActionWidget* ActiveActionUI;

	// --- STATE TRACKING ---
	bool bIsActive;
	bool bIsProcessing;
	float CurrentProgressTime;
	float MaxDuration;

	// --- HIGHLIGHT ENGINE ---
	int32 StencilID;
	float LastHighlightValue;
	bool bWasRising;

	// --- CORE LOGIC HANDLERS ---
	UFUNCTION()
	void HandleExecuteClicked();

	UFUNCTION()
	void HandleCompletedClicked();

	void ExecuteTeleport();
	void ExecuteAnimations();
	void ExecuteCustomTimer();

	// --- VISUAL HELPERS ---
	void EnsureInitialized();
	void ToggleTether(bool bState);
	void ToggleHighlight(bool bState);
	void UpdatePointer(bool bIsActiveMode);

	UPrimitiveComponent* FindMeshByName(FName Name);
	UWidgetComponent* FindWidgetByName(FName Name);
	UAzr_Pointer* FindPlayerPointer() const;
	FVector CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos, const FAzr_TetherConfig& Config);
};