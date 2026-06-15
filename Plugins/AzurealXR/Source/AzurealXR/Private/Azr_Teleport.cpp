

#include "Azr_Teleport.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "NavigationSystem.h" 
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Azr_Pawn.h"

UAzr_Teleport::UAzr_Teleport()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	bIsAiming = false;
	bHasValidLocation = false;
	MaxTeleportDistance = 1000.0f;
	FadeDuration = 0.2f;

	ReticleScale = FVector(0.5f, 0.5f, 0.05f);
	BeamScale = FVector(0.04f, 0.04f, 1.0f);
}

void UAzr_Teleport::BeginPlay()
{
	Super::BeginPlay();
	EnsureInitialized();
}

void UAzr_Teleport::EnsureInitialized()
{
	AActor* Owner = GetOwner();
	if (!Owner || ReticleComponent) return;

	USceneComponent* Root = Owner->GetRootComponent();

	// --- 1. SETUP RETICLE ---
	ReticleComponent = NewObject<UStaticMeshComponent>(Owner, TEXT("TeleportReticle"));
	ReticleComponent->SetMobility(EComponentMobility::Movable);
	ReticleComponent->RegisterComponent();

	if (Root) ReticleComponent->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	if (ReticleMeshAsset) ReticleComponent->SetStaticMesh(ReticleMeshAsset);
	if (ValidReticleMaterial) ReticleComponent->SetMaterial(0, ValidReticleMaterial);

	ReticleComponent->SetVisibility(false);
	ReticleComponent->SetCollisionProfileName(TEXT("NoCollision"));
	ReticleComponent->SetCastShadow(false);
	ReticleComponent->SetWorldScale3D(ReticleScale);

	// --- 2. SETUP SPLINE ---
	ArcSpline = NewObject<USplineComponent>(Owner, TEXT("ArcSpline"));
	ArcSpline->SetMobility(EComponentMobility::Movable);
	ArcSpline->RegisterComponent();
	if (Root) ArcSpline->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	ArcSpline->SetVisibility(false);

	// --- 3. MESH POOL ---
	SplineMeshPool.Empty();
	for (int32 i = 0; i < 30; i++)
	{
		FString MeshName = FString::Printf(TEXT("BeamSegment_%d"), i);
		USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(Owner, FName(*MeshName));

		Segment->SetMobility(EComponentMobility::Movable);
		Segment->RegisterComponent();
		if (Root) Segment->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

		Segment->SetCollisionProfileName(TEXT("NoCollision"));
		Segment->SetCastShadow(false);
		Segment->SetVisibility(false);

		if (BeamMeshAsset) Segment->SetStaticMesh(BeamMeshAsset);
		if (ValidBeamMaterial) Segment->SetMaterial(0, ValidBeamMaterial);

		Segment->SetForwardAxis(ESplineMeshAxis::Z);
		Segment->SetStartScale(FVector2D(BeamScale.X, BeamScale.Y));
		Segment->SetEndScale(FVector2D(BeamScale.X, BeamScale.Y));

		SplineMeshPool.Add(Segment);
	}
}

void UAzr_Teleport::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsAiming && CurrentController)
	{
		UpdateArc();
	}
}

void UAzr_Teleport::HandleTeleportInput(float InputValue, USceneComponent* ControllerSource)
{
	if (!bIsTeleportEnabled) return;
	EnsureInitialized();

	if (InputValue > 0.5f)
	{
		// Prevent hand-switching glitches. Only allow a new controller to aim if we aren't already aiming!
		if (!bIsAiming)
		{
			CurrentController = ControllerSource;
			StartAiming();
			SetComponentTickEnabled(true);
		}
	}
	else
	{
		// Only the hand that started the aim is allowed to release and teleport.
		if (bIsAiming && CurrentController == ControllerSource)
		{
			StopAiming(bHasValidLocation);
			SetComponentTickEnabled(false);
			CurrentController = nullptr; // Clear the memory
		}
	}
}

void UAzr_Teleport::StartAiming()
{
	bIsAiming = true;
	if (ReticleComponent) ReticleComponent->SetVisibility(true);
}

void UAzr_Teleport::StopAiming(bool bExecuteTeleport)
{
	bIsAiming = false;

	if (ReticleComponent) ReticleComponent->SetVisibility(false);
	for (USplineMeshComponent* Segment : SplineMeshPool)
	{
		Segment->SetVisibility(false);
	}

	if (bExecuteTeleport) PerformTeleport();
}

void UAzr_Teleport::UpdateArc()
{
	if (!CurrentController) return;

	FPredictProjectilePathParams Params;
	Params.StartLocation = CurrentController->GetComponentLocation();
	Params.LaunchVelocity = CurrentController->GetForwardVector() * MaxTeleportDistance;
	Params.bTraceWithCollision = true;
	Params.ProjectileRadius = 2.0f;
	Params.MaxSimTime = 2.0f;

	// Set SimFrequency to match Mesh Pool size (15Hz * 2.0s = 30 points)
	Params.SimFrequency = 15.0f;

	Params.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	Params.DrawDebugType = EDrawDebugTrace::None;

	FPredictProjectilePathResult Result;
	bool bHit = UGameplayStatics::PredictProjectilePath(this, Params, Result);

	if (bHit && Result.HitResult.bBlockingHit)
	{
		if (ReticleComponent)
		{
			ReticleComponent->SetWorldLocation(Result.HitResult.Location);

			// Visual Polish - Align to Normal
			FQuat SurfaceRot = FRotationMatrix::MakeFromZ(Result.HitResult.Normal).ToQuat();
			ReticleComponent->SetWorldRotation(SurfaceRot);

			ReticleComponent->SetVisibility(true);
		}

		FNavLocation NavLoc;
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());

		if (NavSys && NavSys->ProjectPointToNavigation(Result.HitResult.Location, NavLoc, FVector(50, 50, 50)))
		{
			bHasValidLocation = true;
			ValidTeleportLocation = NavLoc.Location;
		}
		else
		{
			bHasValidLocation = false;
		}
	}
	else
	{
		bHasValidLocation = false;
		if (ReticleComponent) ReticleComponent->SetVisibility(false);
	}

	TArray<FVector> PathPoints;
	for (const FPredictProjectilePathPointData& PointData : Result.PathData)
	{
		PathPoints.Add(PointData.Location);
	}

	DrawBeam(PathPoints);
	UpdateVisualsColor();
}

void UAzr_Teleport::UpdateVisualsColor()
{
	UMaterialInterface* TargetReticleMat = bHasValidLocation ? ValidReticleMaterial : InvalidReticleMaterial;
	UMaterialInterface* TargetBeamMat = bHasValidLocation ? ValidBeamMaterial : InvalidBeamMaterial;

	if (ReticleComponent && TargetReticleMat)
	{
		ReticleComponent->SetMaterial(0, TargetReticleMat);
	}

	if (TargetBeamMat)
	{
		for (USplineMeshComponent* Segment : SplineMeshPool)
		{
			if (Segment->IsVisible())
			{
				Segment->SetMaterial(0, TargetBeamMat);
			}
		}
	}
}

void UAzr_Teleport::DrawBeam(const TArray<FVector>& PathPoints)
{
	if (!ArcSpline) return;

	// 1. Update Spline (World Space)
	ArcSpline->ClearSplinePoints(false);
	for (const FVector& Point : PathPoints)
	{
		ArcSpline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
	}
	ArcSpline->UpdateSpline();

	int32 NumSegments = PathPoints.Num() - 1;

	for (int32 i = 0; i < SplineMeshPool.Num(); i++)
	{
		USplineMeshComponent* MeshSegment = SplineMeshPool[i];

		if (i < NumSegments)
		{
			MeshSegment->SetVisibility(true);

			// 2. Get Points in World Space
			FVector WorldStartPos = ArcSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
			FVector WorldStartTan = ArcSpline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
			FVector WorldEndPos = ArcSpline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::World);
			FVector WorldEndTan = ArcSpline->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::World);

			// Convert World -> Local to prevent "Double Movement" floating
			FTransform CompTransform = MeshSegment->GetComponentTransform();

			FVector LocalStartPos = CompTransform.InverseTransformPosition(WorldStartPos);
			FVector LocalStartTan = CompTransform.InverseTransformVector(WorldStartTan);
			FVector LocalEndPos = CompTransform.InverseTransformPosition(WorldEndPos);
			FVector LocalEndTan = CompTransform.InverseTransformVector(WorldEndTan);

			MeshSegment->SetStartAndEnd(LocalStartPos, LocalStartTan, LocalEndPos, LocalEndTan);
		}
		else
		{
			MeshSegment->SetVisibility(false);
		}
	}
}

void UAzr_Teleport::PerformTeleport()
{
	APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (!PawnOwner) return;

	APlayerController* PC = Cast<APlayerController>(PawnOwner->GetController());
	if (!PC || !PC->PlayerCameraManager) return;

	// Clear existing timer to prevent spam/overlap
	GetWorld()->GetTimerManager().ClearTimer(TeleportTimerHandle);

	PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeDuration, FLinearColor::Black, false, true);

	TWeakObjectPtr<APawn> WeakPawn(PawnOwner);
	TWeakObjectPtr<APlayerController> WeakPC(PC);

	float DurationCapture = FadeDuration;
	FVector LocCapture = ValidTeleportLocation;

	GetWorld()->GetTimerManager().SetTimer(TeleportTimerHandle, [WeakPawn, WeakPC, LocCapture, DurationCapture]()
		{
			// Safe Execution: No 'this' pointer required.

			// 1. Teleport Pawn
			if (APawn* StrongPawn = WeakPawn.Get())
			{
				if (AAzr_Pawn* AzrPawn = Cast<AAzr_Pawn>(StrongPawn))
				{
					// --- THE ROOM-SCALE FIX ---
					AzrPawn->ExecuteRoomScaleTeleport(LocCapture);
				}
				else
				{
					StrongPawn->SetActorLocation(LocCapture);
				}
			}

			// 2. Fade In
			if (APlayerController* StrongPC = WeakPC.Get())
			{
				if (StrongPC->PlayerCameraManager)
				{
					StrongPC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, DurationCapture, FLinearColor::Black, false, false);
				}
			}

		}, FadeDuration, false);
}

void UAzr_Teleport::TeleportToLocation(FVector TargetLocation)
{
	ValidTeleportLocation = TargetLocation;
	PerformTeleport();
}

void UAzr_Teleport::EnableTeleport()
{
	bIsTeleportEnabled = true;
}

void UAzr_Teleport::DisableTeleport()
{
	bIsTeleportEnabled = false;

	if (bIsAiming)
	{
		StopAiming(false);
		SetComponentTickEnabled(false);
	}
}