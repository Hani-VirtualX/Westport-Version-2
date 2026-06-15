

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
// Required for the Indicator Ghost system (SCS Processor)
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Azr_TouchZone.generated.h"

// Forward Declaration
class AAzr_Indicator;

/**
 * UAzr_TouchZone
 * A smart collision box that automatically renders a perfect 1:1 editor preview
 * of the assigned Touch Indicator, and acts as the interaction anchor for UAzr_Touch.
 */
UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent, DisplayName = "Azr Touch Zone"))
class AZUREALXR_API UAzr_TouchZone : public UBoxComponent
{
	GENERATED_BODY()

public:
	UAzr_TouchZone();

	// --- TOUCH INDICATOR ---
	// The designer assigns the UI/Indicator Blueprint here.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Zone Configuration")
	TSubclassOf<AAzr_Indicator> TouchIndicatorClass;

	// Matches the InteractID on the Azr_Touch actor component to establish a link
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Zone Configuration")
	int32 InteractID = 1;

	// Runtime pointer to the spawned indicator instance
	UPROPERTY(BlueprintReadOnly, Category = "Touch Zone Configuration")
	AAzr_Indicator* SpawnedIndicator;

protected:
	// --- LIFECYCLE ---
	// Added to handle the runtime spawning of the indicator
	virtual void BeginPlay() override;

	// --- USceneComponent Overrides for Editor Preview Stability ---
	virtual void OnRegister() override;
	virtual void OnComponentCreated() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// --- EDITOR PREVIEW SYSTEM ---
	// Marked Transient and TextExportTransient to stabilize visuals in the viewport
	UPROPERTY(Transient, TextExportTransient)
	TArray<UStaticMeshComponent*> PreviewIndicatorMeshes;

	void GenerateIndicatorPreview();
	void ClearIndicatorPreview();
};