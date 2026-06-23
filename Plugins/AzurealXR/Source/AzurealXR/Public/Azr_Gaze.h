

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Azr_Types.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Azr_Gaze.generated.h"

class UAzr_Pointer;
class UMaterialParameterCollection;
class AAzr_Indicator;
class UAzr_Gaze; // Forward declaration needed for the delegate

// We added the UAzr_Gaze* parameter so the Manager knows WHICH zone triggered
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGazeEvent, UAzr_Gaze*, TriggeredZone);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGazeProgressEvent, float, Progress);

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

	virtual void OnRegister() override;
	virtual void OnComponentCreated() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:

	void ToggleHighlight(bool bState);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration")
	int32 InteractID = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration")
	TSubclassOf<AAzr_Indicator> GazeIndicatorClass;

	UPROPERTY(BlueprintReadOnly, Category = "Gaze Configuration")
	AAzr_Indicator* SpawnedIndicator;

	// --- VISUAL CONFIG ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Visuals")
	EAzr_HighlightMode HighlightMode = EAzr_HighlightMode::AllComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Visuals", meta = (EditCondition = "HighlightMode == EAzr_HighlightMode::TargetMeshOnly", EditConditionHides))
	FName TargetMeshName = "TargetMesh";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration")
	float GazeDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration")
	float DrainRate = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration")
	bool bResetOnTrigger = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Visuals")
	float HighlightSpeed = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Audio")
	USoundBase* SoundHighlightStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Audio")
	USoundBase* SoundHighlightEnd;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Audio")
	USoundBase* SoundGazeStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze Configuration|Audio")
	USoundBase* SoundGazeCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnGazeEvent OnGazeTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Azureal|Events")
	FOnGazeProgressEvent OnGazeProgressUpdated;

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void EnableGaze();

	UFUNCTION(BlueprintCallable, Category = "Azureal|Logic")
	void DisableGaze();

	void SetIsBeingLookedAt();
	void UpdatePointer();

private:
	UPROPERTY()
	UMaterialParameterCollection* HighlightMPC;

	UPROPERTY(Transient, TextExportTransient)
	TArray<UStaticMeshComponent*> PreviewIndicatorMeshes;

	void GenerateIndicatorPreview();
	void ClearIndicatorPreview();

	bool bIsGazeEnabled = false;
	bool bIsBeingLookedAt = false;
	bool bWasLookedAt = false;
	bool bHasTriggered = false;
	float CurrentChargeTime = 0.0f;

	int32 StencilID;
	float LastHighlightValue;
	bool bWasRising;

	// Updated to UMeshComponent to bypass invisible collision boxes
	UPROPERTY()
	UMeshComponent* TargetMesh;

	void EnsureInitialized();
	
	void UpdatePointerZOffset(bool bIsGazeMode);
	UMeshComponent* FindMeshByName(FName Name);
	UAzr_Pointer* FindPlayerPointer() const;
};