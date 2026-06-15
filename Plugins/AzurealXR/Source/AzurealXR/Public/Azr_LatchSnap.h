

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Azr_LatchSnap.generated.h"

UCLASS(ClassGroup = (AzurealXR), meta = (BlueprintSpawnableComponent))
class AZUREALXR_API UAzr_LatchSnap : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UAzr_LatchSnap();

	// --- THE MODULAR ID ---
	// Matches the InteractID on the Azr_Latch and Azr_LatchZone to establish a link.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Snap")
	int32 InteractID = 1;

	// Check this box in the editor to make it a Right Hand. Uncheck for Left Hand.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch Snap")
	bool bIsRightHand = false; // Defaulting to Left Hand

#if WITH_EDITOR
	// This magically updates the mesh in the viewport the second you click the boolean
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// We cache the meshes here so we can swap them instantly
	UPROPERTY()
	UStaticMesh* RightHandMeshAsset;

	UPROPERTY()
	UStaticMesh* LeftHandMeshAsset;
};