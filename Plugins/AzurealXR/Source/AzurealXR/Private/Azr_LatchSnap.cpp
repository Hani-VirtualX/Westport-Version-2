

#include "Azr_LatchSnap.h"
#include "UObject/ConstructorHelpers.h"

UAzr_LatchSnap::UAzr_LatchSnap()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 1. Optimize for VR Ghost Hands (No Physics, No Shadows)
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionProfileName(FName("NoCollision"));
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);

	// 2. Hide in game by default so it only appears when the player grabs it
	SetHiddenInGame(true);

	// 3. Automatically find and load the Right Hand Mesh
	// IMPORTANT: Paste your actual Right Hand mesh path here!
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RightMeshFinder(TEXT("/AzurealXR/Interaction/Hands/SM_GhostHand_R"));
	if (RightMeshFinder.Succeeded())
	{
		RightHandMeshAsset = RightMeshFinder.Object;
	}

	// 4. Automatically find and load the Left Hand Mesh
	// IMPORTANT: Paste your actual Left Hand mesh path here!
	static ConstructorHelpers::FObjectFinder<UStaticMesh> LeftMeshFinder(TEXT("/AzurealXR/Interaction/Hands/SM_GhostHand_L"));
	if (LeftMeshFinder.Succeeded())
	{
		LeftHandMeshAsset = LeftMeshFinder.Object;
	}

	// 5. Default to the Left Hand when first spawned since bIsRightHand is false
	if (LeftHandMeshAsset)
	{
		SetStaticMesh(LeftHandMeshAsset);
	}
}

#if WITH_EDITOR
void UAzr_LatchSnap::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Get the name of the variable that the developer just changed in the Details Panel
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// If they clicked the bIsRightHand checkbox...
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAzr_LatchSnap, bIsRightHand))
	{
		// Instantly swap the mesh in the viewport to match their choice!
		if (bIsRightHand && RightHandMeshAsset)
		{
			SetStaticMesh(RightHandMeshAsset);
		}
		else if (!bIsRightHand && LeftHandMeshAsset)
		{
			SetStaticMesh(LeftHandMeshAsset);
		}
	}
}
#endif