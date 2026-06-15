

#include "Azr_GazeWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

void UAzr_GazeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Safely create and cache the Dynamic Material Instance when the widget spawns
	if (ProgressImage)
	{
		// GetDynamicMaterial automatically creates an MID if one doesn't exist yet, 
		// or returns the existing one based on the Image's brush.
		ProgressMID = ProgressImage->GetDynamicMaterial();
	}
}

void UAzr_GazeWidget::SetProgress(float InProgress)
{
	// 1. Fallback in case the material wasn't ready during NativeConstruct
	if (!ProgressMID && ProgressImage)
	{
		ProgressMID = ProgressImage->GetDynamicMaterial();
	}

	// 2. Push the float directly into the Material Graph's "Progress" parameter
	if (ProgressMID)
	{
		// Ensure the FName exactly matches the Scalar Parameter in your Material!
		ProgressMID->SetScalarParameterValue(FName("Progress"), InProgress);
	}
}