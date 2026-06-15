

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Azr_Types.h" // Needed for EAzr_HighlightMode and FAzr_TetherConfig

#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

#include "Azr_Gaze.generated.h"

// Forward Declarations
class UCableComponent;
class UAzr_Pointer;
class UMaterialParameterCollection;
class AAzr_Indicator;

// --- EVENTS ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGazeEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGazeProgressEvent, float, Progress);

/**
 * UAzr_Gaze
 * A smart, gaze-detecting collision volume with full AzurealXR visual integration.
 * Fills a progress variable when looked at and manages world-space context UI (Tethers/Highlights).
 */
UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent, DisplayName = "Azr Gaze Zone"))
class AZUREALXR_API UAzr_Gaze : public UBoxComponent
{
	GENERATED_BODY()

public:
	UAzr_Gaze();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- EDITOR PREVIEW OVERRIDES ---
	virtual void OnRegister() override;
	virtual void OnComponentCreated() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	// --- THE MODULAR ID ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration")
	int32 InteractID = 1;

	// --- GAZE INDICATOR ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration")
	TSubclassOf<AAzr_Indicator> GazeIndicatorClass;

	UPROPERTY(BlueprintReadOnly, Category = "Gaze Configuration")
	AAzr_Indicator* SpawnedIndicator;

	// --- GAZE MATH SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration")
	float GazeDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration")
	float DrainRate = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration")
	bool bResetOnTrigger = true;

	// --- VISUAL CONFIG (AZUREAL STANDARD) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Visuals")
	FName TargetMeshName = "TargetMesh";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Visuals")
	EAzr_HighlightMode HighlightMode = EAzr_HighlightMode::AllComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Visuals")
	float HighlightSpeed = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Visuals")
	FAzr_TetherConfig TetherSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Visuals", meta = (MultiLine = true))
	FText GazeDescription;

	// --- AUDIO ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Audio")
	USoundBase* SoundHighlightStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Audio")
	USoundBase* SoundHighlightEnd;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Audio")
	USoundBase* SoundGazeStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Audio")
	USoundBase* SoundGazeCompleted;

	// --- EVENTS ---
	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnGazeEvent OnGazeTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnGazeProgressEvent OnGazeProgressUpdated;

	// --- API ---
	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableGaze();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisableGaze();

	void SetIsBeingLookedAt();

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

	// --- EDITOR PREVIEW SYSTEM ---
	UPROPERTY(Transient, TextExportTransient)
	TArray<UStaticMeshComponent*> PreviewIndicatorMeshes;

	void GenerateIndicatorPreview();
	void ClearIndicatorPreview();

	// --- STATE ---
	bool bIsGazeEnabled = false;
	bool bIsBeingLookedAt = false;
	bool bWasLookedAt = false;
	bool bHasTriggered = false;
	float CurrentChargeTime = 0.0f;

	int32 StencilID;
	float LastHighlightValue;
	bool bWasRising;

	UPROPERTY()
	UPrimitiveComponent* TargetMesh;
	UPROPERTY()
	USceneComponent* CurrentTargetWidget;

	// --- HELPERS ---
	void EnsureInitialized();
	void ToggleTether(bool bState);
	void ToggleHighlight(bool bState);
	void UpdatePointer(bool bIsLooking);
	void UpdatePointerZOffset(bool bIsGazeMode);
	UPrimitiveComponent* FindMeshByName(FName Name);
	USceneComponent* FindWidgetByName(FName Name);
	UAzr_Pointer* FindPlayerPointer() const;
	FVector CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos, const FAzr_TetherConfig& Config);
};