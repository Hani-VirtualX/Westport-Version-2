

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
// Required for the Indicator Ghost system (SCS Processor)
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Azr_LatchZone.generated.h"

// Forward Declarations
class AAzr_Indicator;
class UAzr_Latch; // <-- ADDED: So the Zone knows what a Brain is

/**
 * UAzr_LatchZone
 * A smart collision box that automatically renders a perfect 1:1 editor preview
 * of the assigned Latch Indicator, and acts as the interaction anchor for UAzr_Latch.
 */
UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent, DisplayName = "Azr Latch Zone"))
class AZUREALXR_API UAzr_LatchZone : public UBoxComponent
{
	GENERATED_BODY()

public:
	UAzr_LatchZone();

	// --- THE MODULAR ID ---
	// Matches the InteractID on the Azr_Latch and Azr_LatchSnap to establish a link.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Zone Configuration")
	int32 InteractID = 1;

	// --- LATCH INDICATOR ---
	// The designer assigns the UI/Indicator Blueprint here.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Zone Configuration")
	TSubclassOf<AAzr_Indicator> LatchIndicatorClass;

	// --- SMART HUB MEMORY ---
	// The currently active Brain. The Hand Scanner reads this to know who is in charge!
	UPROPERTY(BlueprintReadOnly, Category = "Latch Zone State")
	UAzr_Latch* CurrentActiveLatch;

	// The UI Indicator instance that this zone now manages independently.
	UPROPERTY(BlueprintReadOnly, Category = "Latch Zone State")
	AAzr_Indicator* SpawnedLatchIndicator;

protected:
	// --- LIFECYCLE ---
	// Added so the Zone can spawn its own Indicator at the start of the game
	virtual void BeginPlay() override;

	// --- USceneComponent Overrides for Editor Preview Stability ---
	virtual void OnRegister() override;
	virtual void OnComponentCreated() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// --- SMART HUB OVERLAPS ---
	// Migrated from Azr_Latch so the Zone manages its own UI state
	UFUNCTION()
	void OnLatchZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnLatchZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// --- EDITOR PREVIEW SYSTEM ---
	// Marked Transient and TextExportTransient to stabilize visuals in the viewport
	UPROPERTY(Transient, TextExportTransient)
	TArray<UStaticMeshComponent*> PreviewIndicatorMeshes;

	void GenerateIndicatorPreview();
	void ClearIndicatorPreview();
};