

#include "Azr_GazeManager.h"
#include "Azr_Gaze.h"

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

	if (Mode == EAzr_ManagerMode::Sequential)
	{
		// --- SEQUENTIAL LOGIC ---
		if (ActiveGazeZones.IsValidIndex(CurrentIndex) && ActiveGazeZones[CurrentIndex] == TriggeredZone)
		{
			TriggeredZone->DisableGaze();
			CurrentIndex++;

			if (ActiveGazeZones.IsValidIndex(CurrentIndex) && ActiveGazeZones[CurrentIndex])
			{
				ActiveGazeZones[CurrentIndex]->EnableGaze();
			}
			else
			{
				DisableManager(); // All done!
			}
		}
	}
	else if (Mode == EAzr_ManagerMode::NonSequential)
	{
		// --- NON-SEQUENTIAL LOGIC ---

		TriggeredZone->DisableGaze();
		CompletedCount++;

		// 1. Remove the finished zone from our random selection pool
		RemainingZones.Remove(TriggeredZone);

		// 2. Check if we are totally done
		if (CompletedCount >= ActiveGazeZones.Num())
		{
			DisableManager(); // All done!
		}
		else if (RemainingZones.Num() > 0)
		{
			// 3. We have survivors! Pick a random one.
			int32 RandomIndex = FMath::RandRange(0, RemainingZones.Num() - 1);

			// 4. Snap the pointer directly to the random surviving zone
			RemainingZones[RandomIndex]->UpdatePointer();
		}
	}
}