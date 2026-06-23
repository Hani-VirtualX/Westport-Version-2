

#include "Azr_GazeManager.h"
#include "Azr_Gaze.h"
#include "Azr_Pawn.h"                           
#include "Azr_Pointer.h"                        
#include "Kismet/GameplayStatics.h"             
#include "Components/WidgetComponent.h"



UAzr_GazeManager::UAzr_GazeManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAzr_GazeManager::EnableManager()
{
	if (bIsManagerActive || GazeList.Num() == 0) return;
	bIsManagerActive = true;
	CurrentIndex = 0;
	CompletedCount = 0;

	// 1. Resolve the clean UI references into actual usable pointers
	ActiveGazeZones.Empty();
	for (const FComponentReference& Ref : GazeList)
	{
		// GetComponent grabs it securely from the Blueprint Owner using the custom name
		if (UAzr_Gaze* Zone = Cast<UAzr_Gaze>(Ref.GetComponent(GetOwner())))
		{
			ActiveGazeZones.Add(Zone);
		}
	}

	if (ActiveGazeZones.Num() == 0) return;

	RemainingZones = ActiveGazeZones;

	// 2. Bind listeners and reset states
	for (UAzr_Gaze* Zone : ActiveGazeZones)
	{
		// The Manager quietly listens to the zone's event
		Zone->OnGazeTriggered.AddUniqueDynamic(this, &UAzr_GazeManager::HandleZoneTriggered);
		Zone->DisableGaze(); // Ensure everything is off first
	}

	// 3. Execute Mode Logic
	if (Mode == EAzr_ManagerMode::NonSequential)
	{
		// Turn everything on at once
		for (UAzr_Gaze* Zone : ActiveGazeZones)
		{
			Zone->EnableGaze();
		}
	}
	else if (Mode == EAzr_ManagerMode::Sequential)
	{
		// Turn on ONLY the first one
		ActiveGazeZones[0]->EnableGaze();
	}
}

void UAzr_GazeManager::DisableManager()
{
	if (!bIsManagerActive) return;
	bIsManagerActive = false;

	for (UAzr_Gaze* Zone : ActiveGazeZones)
	{
		if (Zone)
		{
			Zone->DisableGaze();
			Zone->OnGazeTriggered.RemoveDynamic(this, &UAzr_GazeManager::HandleZoneTriggered);
		}
	}
}

void UAzr_GazeManager::HandleZoneTriggered(UAzr_Gaze* TriggeredZone)
{
	if (!bIsManagerActive) return;

	TriggeredZone->DisableGaze();
	CompletedCount++;

	// 1. Check if the entire sequence is finished
	if (CompletedCount >= ActiveGazeZones.Num())
	{
		DisableManager();
		return;
	}

	// 2. We are NOT done. Pause the Manager and wait for the Blueprint!
	bIsWaitingForNext = true;

	if (Mode == EAzr_ManagerMode::Sequential)
	{
		// Just cue up the next index. (EnableGaze will be called later)
		CurrentIndex++;
	}
	else if (Mode == EAzr_ManagerMode::NonSequential)
	{
		RemainingZones.Remove(TriggeredZone);

		// Put all remaining zones to sleep so the player can't trigger them early
		for (UAzr_Gaze* Zone : RemainingZones)
		{
			if (Zone)
			{
				Zone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Zone->ToggleHighlight(false);
			}
		}

		// Force hide the global pointer and UI during the pause
		if (AAzr_Pawn* Pawn = Cast<AAzr_Pawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (UAzr_Pointer* Pointer = Pawn->FindComponentByClass<UAzr_Pointer>()) Pointer->DisablePointer();
			if (Pawn->GazeReticleWidget) Pawn->GazeReticleWidget->SetVisibility(false);
		}
	}
}

void UAzr_GazeManager::TriggerNextGaze()
{
	// Don't do anything if we aren't actively waiting for a trigger
	if (!bIsManagerActive || !bIsWaitingForNext) return;

	bIsWaitingForNext = false;

	if (Mode == EAzr_ManagerMode::Sequential)
	{
		// Wake up the next sequential zone
		if (CurrentIndex < ActiveGazeZones.Num())
		{
			ActiveGazeZones[CurrentIndex]->EnableGaze();
		}
	}
	else if (Mode == EAzr_ManagerMode::NonSequential)
	{
		// Wake the remaining zones back up
		for (UAzr_Gaze* Zone : RemainingZones)
		{
			if (Zone)
			{
				// Restore interaction and visuals
				Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				Zone->ToggleHighlight(true);
			}
		}

		// Bring the Reticle UI back
		if (AAzr_Pawn* Pawn = Cast<AAzr_Pawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (Pawn->GazeReticleWidget) Pawn->GazeReticleWidget->SetVisibility(true);
		}

		// Snap the pointer to a new random target
		if (RemainingZones.Num() > 0)
		{
			int32 RandomIndex = FMath::RandRange(0, RemainingZones.Num() - 1);
			RemainingZones[RandomIndex]->UpdatePointer();
		}
	}
}