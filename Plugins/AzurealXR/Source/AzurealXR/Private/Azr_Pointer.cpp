

#include "Azr_Pointer.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMathLibrary.h"

UAzr_Pointer::UAzr_Pointer()
{
	PrimaryComponentTick.bCanEverTick = true;
	// OPTIMIZATION: Start asleep. Only wake up when tracking something.
	PrimaryComponentTick.bStartWithTickEnabled = false;

	PointerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PointerMesh"));
	PointerMesh->SetupAttachment(this);
	PointerMesh->SetVisibility(false);
	PointerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TrackingMode = 0;
}

void UAzr_Pointer::BeginPlay()
{
	Super::BeginPlay();
	// Ensure we start clean
	DisablePointer();
}

void UAzr_Pointer::EnablePointer_TargetComponent(USceneComponent* Target)
{
	if (!Target)
	{
		DisablePointer();
		return;
	}

	// Smart Pointer Assignment (Safe)
	TrackedComponent = Target;
	TrackingMode = 2;

	PointerMesh->SetVisibility(true);

	// Wake up Tick
	SetComponentTickEnabled(true);
}

void UAzr_Pointer::EnablePointer_TargetLocation(FVector TargetLocation)
{
	TrackedLocation = TargetLocation;
	TrackingMode = 1;

	PointerMesh->SetVisibility(true);

	// Wake up Tick
	SetComponentTickEnabled(true);
}

void UAzr_Pointer::DisablePointer()
{
	TrackingMode = 0;
	TrackedComponent = nullptr;

	PointerMesh->SetVisibility(false);

	// Sleepy Principle: Stop ticking to save CPU
	SetComponentTickEnabled(false);
}

void UAzr_Pointer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FVector TargetPos = FVector::ZeroVector;
	bool bHasValidTarget = false;

	// --- 1. GET TARGET POSITION ---
	if (TrackingMode == 2) // Component Mode
	{
		// SAFETY CHECK: IsValid() checks if the object is still alive in memory
		if (TrackedComponent.IsValid())
		{
			// Get() retrieves the raw pointer safely
			TargetPos = TrackedComponent.Get()->GetComponentLocation();
			bHasValidTarget = true;
		}
		else
		{
			// Object was destroyed (e.g., broke/died). 
			// Auto-shutdown immediately to prevent errors.
			DisablePointer();
			return;
		}
	}
	else if (TrackingMode == 1) // Location Mode
	{
		TargetPos = TrackedLocation;
		bHasValidTarget = true;
	}

	if (!bHasValidTarget) return;

	// --- 2. CALCULATE ROTATION ---
	FVector StartPos = GetComponentLocation();

	if (bFlattenZ)
	{
		StartPos.Z = 0.0f;
		TargetPos.Z = 0.0f;
	}

	FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(StartPos, TargetPos);

	// --- 3. APPLY ROTATION ---
	// RInterpTo gives a smooth turning effect
	FRotator NewRot = FMath::RInterpTo(GetComponentRotation(), TargetRot, DeltaTime, RotationSpeed);

	SetWorldRotation(NewRot);
}