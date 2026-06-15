

#include "Azr_HandScanner.h"
#include "Components/PrimitiveComponent.h"
#include "Azr_Interactable.h"
#include "Azr_LatchZone.h" // <-- NEW: Include our Smart Hub
#include "DrawDebugHelpers.h" 

UAzr_HandScanner::UAzr_HandScanner()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	InteractProfile = FName("Azr_Collision");
	bIsRightHand = true;
	bShowDebugVisuals = false;

	InteractCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("InteractCapsule"));
	InteractCapsule->SetUsingAbsoluteLocation(false);
	InteractCapsule->SetUsingAbsoluteRotation(false);
	InteractCapsule->SetupAttachment(this);
	InteractCapsule->SetCapsuleSize(8.0f, 8.0f);
	InteractCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractCapsule->SetCollisionProfileName(InteractProfile);
	InteractCapsule->SetGenerateOverlapEvents(true);
}

void UAzr_HandScanner::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(bShowDebugVisuals);

	if (InteractCapsule)
	{
		InteractCapsule->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
		InteractCapsule->SetRelativeLocation(FVector::ZeroVector);
		InteractCapsule->SetRelativeRotation(FRotator::ZeroRotator);

		InteractCapsule->OnComponentBeginOverlap.AddDynamic(this, &UAzr_HandScanner::OnCapsuleOverlap);
		InteractCapsule->OnComponentEndOverlap.AddDynamic(this, &UAzr_HandScanner::OnCapsuleEndOverlap);
	}
}

void UAzr_HandScanner::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (InteractCapsule && bShowDebugVisuals)
	{
		DrawDebugCapsule(GetWorld(), InteractCapsule->GetComponentLocation(),
			InteractCapsule->GetScaledCapsuleHalfHeight(),
			InteractCapsule->GetScaledCapsuleRadius(),
			InteractCapsule->GetComponentQuat(), FColor::Red, false, -1.0f, 0, 1.0f);

		if (CurrentHoveredComponent.IsValid())
		{
			DrawDebugBox(GetWorld(), CurrentHoveredComponent.Get()->GetOwner()->GetActorLocation(), FVector(11, 11, 11), FColor::Green, false, -1.0f, 0, 2.0f);
		}

		if (CurrentHoveredLatch.IsValid())
		{
			DrawDebugBox(GetWorld(), CurrentHoveredLatch.Get()->GetComponentLocation(), FVector(8, 8, 8), FColor::Blue, false, -1.0f, 0, 2.0f);
		}
	}
}

void UAzr_HandScanner::OnCapsuleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == GetOwner() || !OtherComp) return;
	if (OtherComp->GetCollisionProfileName() != InteractProfile) return;

	// Check for Grabbables (Kept original logic)
	UAzr_Grab* FoundGrab = OtherActor->FindComponentByClass<UAzr_Grab>();
	if (FoundGrab)
	{
		OverlappedGrabbables.AddUnique(FoundGrab);
	}

	// --- THE SMART HUB BRIDGE ---
	// Instead of searching the whole Actor, we ask the exact collision box we touched
	UAzr_LatchZone* FoundZone = Cast<UAzr_LatchZone>(OtherComp);
	if (FoundZone && FoundZone->CurrentActiveLatch)
	{
		// Add the specific Active Brain to our array!
		OverlappedLatches.AddUnique(FoundZone->CurrentActiveLatch);
	}

	UpdateBestCandidate();
}

void UAzr_HandScanner::OnCapsuleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || !OtherComp) return;

	// Cleanup Grabbables (Kept original logic)
	for (int32 i = OverlappedGrabbables.Num() - 1; i >= 0; i--)
	{
		if (!OverlappedGrabbables[i].IsValid() || OverlappedGrabbables[i]->GetOwner() == OtherActor)
			OverlappedGrabbables.RemoveAt(i);
	}

	// --- THE SMART HUB CLEANUP ---
	UAzr_LatchZone* LeftZone = Cast<UAzr_LatchZone>(OtherComp);
	for (int32 i = OverlappedLatches.Num() - 1; i >= 0; i--)
	{
		if (!OverlappedLatches[i].IsValid())
		{
			OverlappedLatches.RemoveAt(i);
			continue;
		}

		// Only remove the latch if it belongs to the specific Zone we just left.
		// (We no longer wipe all latches on the OtherActor! This allows multiple components to thrive.)
		if (LeftZone && OverlappedLatches[i].Get() == LeftZone->CurrentActiveLatch)
		{
			OverlappedLatches.RemoveAt(i);
		}
	}

	UpdateBestCandidate();
}

void UAzr_HandScanner::UpdateBestCandidate()
{
	CurrentHoveredComponent = nullptr;
	for (int32 i = OverlappedGrabbables.Num() - 1; i >= 0; i--)
	{
		if (OverlappedGrabbables[i].IsValid())
		{
			if (OverlappedGrabbables[i]->IsComponentTickEnabled())
			{
				CurrentHoveredComponent = OverlappedGrabbables[i];
				break;
			}
		}
		else { OverlappedGrabbables.RemoveAt(i); }
	}

	CurrentHoveredLatch = nullptr;
	for (int32 i = OverlappedLatches.Num() - 1; i >= 0; i--)
	{
		if (OverlappedLatches[i].IsValid())
		{
			if (OverlappedLatches[i]->IsComponentTickEnabled())
			{
				CurrentHoveredLatch = OverlappedLatches[i];
				break;
			}
		}
		else { OverlappedLatches.RemoveAt(i); }
	}
}

void UAzr_HandScanner::ProcessGrabInput(bool bIsPressed)
{
	if (bIsPressed)
	{
		
		if (bIsGripPressed) return;
		bIsGripPressed = true;

		if (CurrentHoveredLatch.IsValid() && CurrentHoveredLatch->IsComponentTickEnabled() && !CurrentHeldLatch.IsValid())
		{
			CurrentHeldLatch = CurrentHoveredLatch;
			CurrentHeldLatch->GrabLatch(this);
			return;
		}

		if (CurrentHoveredComponent.IsValid() && CurrentHoveredComponent->IsComponentTickEnabled() && !CurrentHeldComponent.IsValid())
		{
			CurrentHeldComponent = CurrentHoveredComponent;
			USceneComponent* TargetSnap = nullptr;
			if (AAzr_Interactable* BaseInteractable = Cast<AAzr_Interactable>(CurrentHeldComponent.Get()->GetOwner()))
			{
				TargetSnap = BaseInteractable->GetSnapPoint(bIsRightHand);
			}
			CurrentHeldComponent.Get()->SnapActorToHand(this, TargetSnap);
		}
	}
	else
	{
		
		bIsGripPressed = false;

		if (CurrentHeldLatch.IsValid())
		{
			CurrentHeldLatch->ReleaseSpecificHand(this);
			CurrentHeldLatch = nullptr;
		}

		if (CurrentHeldComponent.IsValid())
		{
			CurrentHeldComponent.Get()->ReleaseHand();
			CurrentHeldComponent = nullptr;
		}

		UpdateBestCandidate();
	}
}