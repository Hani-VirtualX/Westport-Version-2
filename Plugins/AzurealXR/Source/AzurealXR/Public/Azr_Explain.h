

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Azr_Types.h" 
#include "Azr_ExplainWidget.h" 

#include "Azr_Explain.generated.h"

// Forward Declarations
class UWidgetComponent;
class UAudioComponent;
class UCableComponent;
class UStaticMeshComponent;
class UMaterialParameterCollection;
class UAzr_ExplainWidget;
class UAzr_Pointer; // Standard pointer reference

// --- EVENTS ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExplainFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExplainPlusFinished, int32, StepIndex);

// Fires when player press start button
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExplainStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExplainPlusStarted, int32, StepIndex);

/**
 * UAzr_Explain
 * The logic manager for AzurealXR Explanation UI flows.
 * Handles single-shot explains or multi-step (Start -> Middle -> End) sequences.
 */
UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent, DisplayName = "Azr Explain Logic"))
class AZUREALXR_API UAzr_Explain : public UActorComponent
{
	GENERATED_BODY()

public:
	UAzr_Explain();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// --- CORE SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Settings")
	int32 InteractID = 1;

	// --- 1. NORMAL EXPLAIN ---
	// Used when EnableExplain() is called
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Settings|Normal Explain")
	FAzr_ExplainStep SingleExplainStep;

	// --- 2. EXPLAIN+ (THE CHAIN) ---
	// Used when EnableExplainPlus() is called
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Settings|Explain Plus")
	FAzr_ExplainStep StartStep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Settings|Explain Plus")
	TArray<FAzr_ExplainStep> MiddleSteps;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Settings|Explain Plus")
	FAzr_ExplainStep EndStep;

	// --- AUDIO ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Settings|Audio")
	USoundBase* SoundHighlightStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explain Settings|Audio")
	USoundBase* SoundHighlightEnd;

	// --- EVENTS ---
	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnExplainFinished OnExplainCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnExplainPlusFinished OnExplainPlusCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnExplainStarted OnExplainStarted;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnExplainPlusStarted OnExplainPlusStarted;

	// --- API ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableExplain();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableExplainPlus();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisableExplain();

private:
	// --- INTERNAL COMPONENTS ---
	UPROPERTY()
	UAudioComponent* ActiveAudioComp;
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

	// --- STATE TRACKING ---
	bool bIsActive;
	bool bIsPlusMode;
	int32 CurrentMiddleIndex;
	EAzr_ExplainStepType CurrentStepType;

	bool bIsStepRunning = false;
	float StepStartTime = 0.0f;
	float StepDuration = 0.0f;

	// Stores the visuals of the currently active step so helpers can read it dynamically
	FAzr_ExplainStep CurrentActiveStep;

	UPROPERTY()
	UWidgetComponent* ActiveWidgetComp;

	UPROPERTY()
	UAzr_ExplainWidget* ActiveExplainUI;

	// --- HIGHLIGHT ENGINE ---
	int32 StencilID;
	float LastHighlightValue;
	bool bWasRising;

	// --- CORE LOGIC HANDLERS ---
	void LoadStep(const FAzr_ExplainStep& StepData, EAzr_ExplainStepType StepType);
	void HideAllWidgets();

	UFUNCTION()
	void HandlePlayAudioClicked();
	UFUNCTION()
	void HandleProceedClicked();

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