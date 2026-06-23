

#include "Azr_Latch.h"
#include "Azr_Interactable.h"
#include "Azr_Indicator.h"
#include "Azr_Pointer.h" 
#include "Azr_Pawn.h"        
#include "Azr_HandScanner.h" 
#include "Azr_LatchZone.h" 
#include "Azr_LatchSnap.h" 
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/WidgetComponent.h"
#include "CableComponent.h"
#include "Azr_ExplainWidget.h"
#include "Azr_ActionWidget.h"
#include "Azr_LabelWidget.h"

UAzr_Latch::UAzr_Latch()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	LinkedLatchZone = nullptr;
	CachedPlayerPawn = nullptr;
	CustomLeftSnap = nullptr;
	CustomRightSnap = nullptr;

	// --- DEFAULTS ---
	InteractID = 1; // <-- NEW: The Modular ID
	LatchType = EAzr_LatchType::Angular;
	InteractionAxis = EAzr_Axis::X_Axis;
	StartValue = 0.0f;
	LimitMin = 0.0f;
	LimitMax = 90.0f;
	Damping = 15.0f;

	bUseLatchSnapHand = false;
	HighlightSpeed = 1.2f;
	HandFlyDuration = 0.15f;
	MinLeverArmLength = 5.0f;
	MaxBreakDistance = 50.0f;

	StepSize = 1.0f;
	HapticIntensity = 1.5f;

	LatchTrigger = EAzr_LatchTrigger::Disabled;
	TriggerThreshold = 0.5f;
	bIsTriggerEngaged = false;

	StencilID = 252;
	LastValue = 0.0f;
	bWasRising = false;
	bIsHandFlying = false;
	LastAngle = 0.0f;
	bInvertRotation = false;

	// --- INTERNAL COMPONENTS ---
	StartAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TetherStartAnchor"));
	StartAnchor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StartAnchor->SetCastShadow(false);
	StartAnchor->SetVisibility(false);

	EndAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TetherEndAnchor"));
	EndAnchor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EndAnchor->SetCastShadow(false);
	EndAnchor->SetVisibility(false);

	TetherCable = CreateDefaultSubobject<UCableComponent>(TEXT("TetherCable"));
	TetherCable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TetherCable->SetVisibility(false);
	TetherCable->NumSegments = 20;
	TetherCable->SolverIterations = 4;
	TetherCable->CableLength = 0.0f;

	// --- ASSET INITIALIZATION ---
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPCAsset(TEXT("/AzurealXR/Interaction/Highlight/MPC_Highlight"));
	if (MPCAsset.Succeeded()) HighlightMPC = MPCAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> StartSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_Start"));
	if (StartSoundAsset.Succeeded()) SoundHighlightStart = StartSoundAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> EndSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_End"));
	if (EndSoundAsset.Succeeded()) SoundHighlightEnd = EndSoundAsset.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/AzurealXR/Interaction/Cable_System/CableHead"));
	if (SphereMesh.Succeeded()) TetherSettings.AnchorMesh = SphereMesh.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CableMatAsset(TEXT("/AzurealXR/Interaction/Cable_System/M_Cable"));
	if (CableMatAsset.Succeeded()) TetherSettings.CableMaterial = CableMatAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> GrabSoundAsset(TEXT("/AzurealXR/Interaction/Grab_Latch/SC_Universal_Grab"));
	if (GrabSoundAsset.Succeeded()) SoundOnLatch = GrabSoundAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> ReleaseSoundAsset(TEXT("/AzurealXR/Interaction/Grab_Latch/SC_Universal_Release"));
	if (ReleaseSoundAsset.Succeeded()) SoundOnRelease = ReleaseSoundAsset.Object;

	static ConstructorHelpers::FObjectFinder<UHapticFeedbackEffect_Base> HapticDragAsset(TEXT("/AzurealXR/Interaction/Grab_Latch/Haptic_Latch_Drag"));
	if (HapticDragAsset.Succeeded()) HapticEffect = HapticDragAsset.Object;
}

void UAzr_Latch::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetRelativeTransform();

	EnsureInitialized();
	AutoDetectLatchZone();

	CachedPlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner());

	// --- 1. SCAN FOR CUSTOM LATCH SNAPS ---
	TArray<UAzr_LatchSnap*> FoundSnaps;
	GetOwner()->GetComponents<UAzr_LatchSnap>(FoundSnaps);
	for (UAzr_LatchSnap* Snap : FoundSnaps)
	{
		// NEW: Only claim snaps that match our exact InteractID
		if (Snap && Snap->InteractID == this->InteractID)
		{
			if (Snap->GetAttachParent() == this || Snap->GetAttachParent() == TargetHandleMesh)
			{
				if (Snap->bIsRightHand) CustomRightSnap = Snap;
				else CustomLeftSnap = Snap;
			}
		}
	}

	// --- 2. SMART SPATIAL REPARENTING (The Logic Pivot) ---
	if (TargetHandleMesh)
	{
		auto ProtectScaleAndAttach = [this](USceneComponent* Comp) {
			if (Comp) {
				FTransform OriginalWorldTransform = Comp->GetComponentTransform();
				Comp->SetAbsolute(false, false, true);
				Comp->AttachToComponent(TargetHandleMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				Comp->SetWorldTransform(OriginalWorldTransform);
			}
			};

		if (bUseLatchSnapHand)
		{
			// Complex Mode: Attach our securely ID-matched assets
			if (LinkedLatchZone) ProtectScaleAndAttach(LinkedLatchZone);
			if (CustomLeftSnap) ProtectScaleAndAttach(CustomLeftSnap);
			if (CustomRightSnap) ProtectScaleAndAttach(CustomRightSnap);
		}
		else
		{
			// Simple Mode
			if (Parent)
			{
				ProtectScaleAndAttach(Parent->GrabZone);
				ProtectScaleAndAttach(Parent->LeftSnap);
				ProtectScaleAndAttach(Parent->RightSnap);
			}
		}
	}

	// --- 3. CALCULATE IDEAL GRIP OFFSETS ---
	FTransform HandleTrans = TargetHandleMesh ? TargetHandleMesh->GetComponentTransform() : GetOwner()->GetActorTransform();

	USceneComponent* ActiveLeftSnap = bUseLatchSnapHand ? CustomLeftSnap : (Parent ? Parent->LeftSnap : nullptr);
	USceneComponent* ActiveRightSnap = bUseLatchSnapHand ? CustomRightSnap : (Parent ? Parent->RightSnap : nullptr);

	if (ActiveLeftSnap) IdealLeftGripTransform = ActiveLeftSnap->GetComponentTransform().GetRelativeTransform(HandleTrans);
	if (ActiveRightSnap) IdealRightGripTransform = ActiveRightSnap->GetComponentTransform().GetRelativeTransform(HandleTrans);

	// (NOTE: The Indicator spawning and Overlap binding logic that used to be here has been completely deleted. The Zone handles it now!)
}

// --- INTERFACE ---

void UAzr_Latch::EnableLatch()
{
	if (bIsGrabEnabled) return;
	bIsGrabEnabled = true;

	EnsureInitialized();

	// Failsafe: if we haven't found a zone yet, try to auto-detect one now
	if (!LinkedLatchZone) AutoDetectLatchZone();

	// Always start with the trigger lock engaged (locked) for safety
	bIsTriggerEngaged = false;

	// 1. Initialize the Latch Value
	float PreEnableValue = CurrentRawValue;
	CurrentRawValue = FMath::Clamp(StartValue, LimitMin, LimitMax);
	ApplyConstraints(PreEnableValue);

	// 2. Wake up the Smart Latch Zone & Take Control
	if (LinkedLatchZone)
	{
		// --- THE HANDSHAKE ---
		LinkedLatchZone->CurrentActiveLatch = this;
		LinkedLatchZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		LinkedLatchZone->SetCollisionProfileName(FName("Azr_Collision"));
		LinkedLatchZone->UpdateOverlaps();

		// --- THE FIX: Unhide the Zone's UI ---
		if (LinkedLatchZone->SpawnedLatchIndicator)
		{
			LinkedLatchZone->SpawnedLatchIndicator->SetActorHiddenInGame(false);
		}
	}

	// (NOTE: SpawnedLatchIndicator visibility code deleted. The Zone handles its own UI.)

	// 3. Reset Highlight State
	if (GetWorld()) { LastValue = 0.0f; bWasRising = true; }
	AAzr_Interactable::SetGlobalHiveSpeed(HighlightSpeed);

	// 4. Wake up the Widget FIRST so Slate can start its layout math 
	CurrentTargetWidget = FindWidgetByName(TetherSettings.TargetWidgetName);
	if (CurrentTargetWidget)
	{
		CurrentTargetWidget->SetVisibility(true);

		// --- NEW: INJECT DYNAMIC TEXT FOR ANY WIDGET TYPE ---
		if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(CurrentTargetWidget))
		{
			UUserWidget* UserUI = WidgetComp->GetUserWidgetObject();

			// 1. Is it an Explain Widget?
			if (UAzr_ExplainWidget* ExplainUI = Cast<UAzr_ExplainWidget>(UserUI))
			{
				ExplainUI->SetExplainText(LatchDescription);
			}
			// 2. Is it an Action Widget?
			else if (UAzr_ActionWidget* ActionUI = Cast<UAzr_ActionWidget>(UserUI))
			{
				ActionUI->SetActionDescription(LatchDescription);
			}
			// 3. Is it a Label Widget?
			else if (UAzr_LabelWidget* LabelUI = Cast<UAzr_LabelWidget>(UserUI))
			{
				LabelUI->SetLabelText(LatchDescription);
			}
		}
	}

	// 5. Activate Visuals
	ToggleHighlight(true);
	ToggleTether(true);

	// 6. Register with Pointer System
	UpdatePointer(false);

	SetComponentTickEnabled(true);
}

void UAzr_Latch::DisableLatch()
{
	if (!bIsGrabEnabled) return;

	// --- NEW: AUTO-RELEASE GATE ---
	// If false, the player's ghost hand remains glued to the frozen handle until they let go of the grip button!
	if (bAutoRelease)
	{
		while (ActiveHands.Num() > 0)
		{
			ReleaseSpecificHand(ActiveHands[0]);
		}
	}

	bIsGrabEnabled = false;
	bIsTriggerEngaged = false;
	bHasTetherSettled = false;

	// 1. Disable the Smart Latch Zone safely
	if (LinkedLatchZone)
	{
		if (LinkedLatchZone->CurrentActiveLatch == this)
		{
			LinkedLatchZone->CurrentActiveLatch = nullptr;
			LinkedLatchZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			LinkedLatchZone->SetCollisionProfileName(FName("NoCollision"));

			if (LinkedLatchZone->SpawnedLatchIndicator)
			{
				LinkedLatchZone->SpawnedLatchIndicator->SetActorHiddenInGame(true);
			}
		}
	}

	// 2. Kill Visuals
	ToggleHighlight(false);
	ToggleTether(false);

	// 3. Unregister Pointer
	if (UAzr_Pointer* Pointer = FindPlayerPointer()) Pointer->DisablePointer();

	if (CurrentTargetWidget) CurrentTargetWidget->SetVisibility(false);

	// --- FIX: Only kill the tick if no hands are holding it! ---
	if (ActiveHands.Num() == 0)
	{
		SetComponentTickEnabled(false);
	}
}

// --- NEW: SAFE CONFIG SWAP ---

void UAzr_Latch::SetLatchConfig(EAzr_LatchType NewType, EAzr_Axis NewAxis, float NewMin, float NewMax, float NewStartValue)
{
	// --- THE SAFETY CATCH ---
	if (bIsGrabbed)
	{
		ReleaseLatch();
	}

	// 1. Update the math rules
	LatchType = NewType;
	InteractionAxis = NewAxis;

	// --- UNREAL ENGINE CLAMP SAFETY ---
	// FMath::Clamp breaks if Min is larger than Max. This auto-sorts them so the math never panics.
	LimitMin = FMath::Min(NewMin, NewMax);
	LimitMax = FMath::Max(NewMin, NewMax);

	StartValue = NewStartValue;

	// 2. THE MEMORY FIX
	// We completely REMOVED the InitialTransform overwrite here.
	// InitialTransform MUST remain the absolute zero point set at BeginPlay to prevent Double Transforms!

	// 3. Reset the math tracker
	float PreConfigValue = CurrentRawValue;
	CurrentRawValue = FMath::Clamp(StartValue, LimitMin, LimitMax);

	// 4. Snap to the exact new StartValue immediately
	if (bIsGrabEnabled)
	{
		ApplyConstraints(PreConfigValue);
	}
}

// --- CORE TICK ---

void UAzr_Latch::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsHandFlying)
	{
		HandleHandFly(DeltaTime);
	}

	if (bIsGrabbed && ActiveHands.Num() > 0)
	{
		CheckBreakDistance();

		if (bIsGrabbed)
		{
			// --- THE TRIGGER LOGIC GATE ---
			bool bCanMove = true;

			if (LatchTrigger != EAzr_LatchTrigger::Disabled)
			{
				bCanMove = bIsTriggerEngaged;
			}

			// Only calculate physics if the lock is disengaged
			if (bCanMove)
			{
				// The math always follows the FIRST hand that grabbed (The Leader)
				if (LatchType == EAzr_LatchType::Angular) HandleAngularInteraction(DeltaTime);
				else if (LatchType == EAzr_LatchType::Linear) HandleLinearInteraction(DeltaTime);
				else if (LatchType == EAzr_LatchType::Rotation) HandleRotationInteraction(DeltaTime);
			}
			else
			{
				// Use the Leader hand as the primary math reference while locked to prevent jumping
				if (ActiveHands[0])
				{
					if (LatchType == EAzr_LatchType::Angular) LastAngle = CalculateAngle(ActiveHands[0]->GetComponentLocation());
					else if (LatchType == EAzr_LatchType::Rotation) LastAngle = CalculateTwistAngle(ActiveHands[0]);
					else GrabOffsetValue = CalculateLinearDist(ActiveHands[0]->GetComponentLocation()) - CurrentRawValue;
				}
			}
		}
	}
	else if (bUseAutoReturn && !FMath::IsNearlyEqual(CurrentRawValue, ReturnValue, 0.1f))
	{
		HandleAutoReturn(DeltaTime);
	}

	// Billboard the Widget toward the player
	if (bIsGrabEnabled && CurrentTargetWidget)
	{
		if (APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			FVector StartLoc = CurrentTargetWidget->GetComponentLocation();
			FRotator NewRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, CamManager->GetCameraLocation());
			CurrentTargetWidget->SetWorldRotation(NewRot);
		}

		// --- UPDATED: Dynamically update the tether anchor every frame using the passed config ---
		if (TetherSettings.bEnableTether && EndAnchor)
		{
			FVector DynamicEndPos = CalculateSurfaceAnchor(CurrentTargetWidget, TetherSettings.WidgetAnchorPos, TetherSettings);
			EndAnchor->SetWorldLocation(DynamicEndPos);

			// --- RIGID OVERRIDE ---
			float ActualDistance = FVector::Dist(StartAnchor->GetComponentLocation(), DynamicEndPos);
			float Slack = TetherSettings.CableHang;

			if (Slack <= 0.1f)
			{
				// Force the cable to the exact distance with no gravity
				TetherCable->CableLength = ActualDistance;
				TetherCable->CableGravityScale = 0.0f;
			}
			else
			{
				TetherCable->CableLength = ActualDistance + Slack;
				TetherCable->CableGravityScale = 1.0f;
			}

			// Flush physics momentum to kill wobble
			TetherCable->SetRelativeLocation(FVector::ZeroVector);
			TetherCable->EndLocation = FVector::ZeroVector;
		}
	}

	if (bIsGrabEnabled)
	{
		UpdatePointer(ActiveHands.Num() > 0);
	}

	// Highlight Pulse logic when not being held
	if (bIsGrabEnabled && ActiveHands.Num() == 0 && HighlightMPC)
	{
		if (UWorld* World = GetWorld())
		{
			float CurrentSpeed = AAzr_Interactable::GetGlobalHiveSpeed();
			float Phase = (World->GetTimeSeconds() * CurrentSpeed * UE_TWO_PI) - UE_HALF_PI;
			float CurrentValue = (FMath::Sin(Phase) + 1.0f) / 2.0f;

			UKismetMaterialLibrary::SetScalarParameterValue(World, HighlightMPC, FName("Alpha"), CurrentValue);

			bool bIsRising = (CurrentValue > LastValue);
			if (CurrentValue < 0.05f && bIsRising && !bWasRising && SoundHighlightStart)
			{
				UGameplayStatics::SpawnSoundAttached(SoundHighlightStart, GetOwner()->GetRootComponent());
			}
			if (bWasRising && !bIsRising && SoundHighlightEnd)
			{
				UGameplayStatics::SpawnSoundAttached(SoundHighlightEnd, GetOwner()->GetRootComponent());
			}
			LastValue = CurrentValue;
			bWasRising = bIsRising;
		}
	}
}

void UAzr_Latch::HandleHandFly(float DeltaTime)
{
	if (ActiveGhostHands.Num() == 0) { bIsHandFlying = false; return; }

	HandFlyTime += DeltaTime;
	float Alpha = FMath::Clamp(HandFlyTime / HandFlyDuration, 0.0f, 1.0f);
	float SmoothedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);

	// Update all active ghost handsif (LinkedLatchZone)
	for (USceneComponent* Ghost : ActiveGhostHands)
	{
		if (Ghost)
		{
			// OPTIMIZED: Use your boolean to know exactly which ecosystem to check
			bool bIsRight = false;
			if (bUseLatchSnapHand)
			{
				bIsRight = (Ghost == CustomRightSnap);
			}
			else
			{
				if (AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner()))
				{
					bIsRight = (Ghost == Parent->RightSnap);
				}
			}

			FTransform Target = bIsRight ? IdealRightGripTransform : IdealLeftGripTransform;

			Ghost->SetRelativeTransform(FTransform(
				FQuat::Slerp(HandFlyStartRelative.GetRotation(), Target.GetRotation(), SmoothedAlpha),
				FMath::Lerp(HandFlyStartRelative.GetLocation(), Target.GetLocation(), SmoothedAlpha),
				FVector::OneVector // Maintain 1.0 scale to respect Absolute Scale fix
			));
		}
	}

	if (Alpha >= 1.0f)
	{
		bIsHandFlying = false;
	}
}

// --- TRIGGER INPUT API ---

void UAzr_Latch::HandleTriggerInput(float Value)
{
	if (!bIsGrabbed || ActiveHands.Num() == 0) return;
	if (LatchTrigger == EAzr_LatchTrigger::Disabled) return;

	bool bIsCorrectHand = false;

	for (USceneComponent* Hand : ActiveHands)
	{
		bool bHandIsRight = false;
		if (UAzr_HandScanner* Scanner = Cast<UAzr_HandScanner>(Hand)) bHandIsRight = Scanner->bIsRightHand;
		else bHandIsRight = Hand->ComponentHasTag(FName("Right"));

		if (LatchTrigger == EAzr_LatchTrigger::BothHands) bIsCorrectHand = true;
		else if (LatchTrigger == EAzr_LatchTrigger::LeftHand && !bHandIsRight) bIsCorrectHand = true;
		else if (LatchTrigger == EAzr_LatchTrigger::RightHand && bHandIsRight) bIsCorrectHand = true;
	}

	if (bIsCorrectHand)
	{
		bool bNewEngaged = (Value >= TriggerThreshold);

		// Math baseline reset to prevent snapping when pulling trigger
		if (bNewEngaged && !bIsTriggerEngaged)
		{
			USceneComponent* PrimaryHand = ActiveHands[0];
			if (LatchType == EAzr_LatchType::Angular) LastAngle = CalculateAngle(PrimaryHand->GetComponentLocation());
			else if (LatchType == EAzr_LatchType::Rotation) LastAngle = CalculateTwistAngle(PrimaryHand);
			else GrabOffsetValue = CalculateLinearDist(PrimaryHand->GetComponentLocation()) - CurrentRawValue;
		}

		bIsTriggerEngaged = bNewEngaged;
	}
}

// --- INTERACTION EVENTS ---

void UAzr_Latch::GrabLatch(USceneComponent* Hand)
{
	if (!bIsGrabEnabled || !Hand || ActiveHands.Contains(Hand)) return;

	// --- 1. ALLOWED HAND SECURITY GATE ---
	bool bIsRight = false;
	if (UAzr_HandScanner* Scanner = Cast<UAzr_HandScanner>(Hand)) bIsRight = Scanner->bIsRightHand;
	else bIsRight = Hand->ComponentHasTag(FName("Right"));

	if (AllowedGrabHand == EAzr_AllowedHand::LeftHand && bIsRight) return;
	if (AllowedGrabHand == EAzr_AllowedHand::RightHand && !bIsRight) return;

	// --- GRAB APPROVED ---
	bIsGrabbed = true;
	ActiveHands.Add(Hand);
	bIsTriggerEngaged = false;

	SetComponentTickEnabled(true);

	// --- HAPTIC: THE HANDSHAKE (VR Native) ---
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		EControllerHand TargetHand = bIsRight ? EControllerHand::Right : EControllerHand::Left;

		if (HapticEffect)
		{
			PC->PlayHapticEffect(HapticEffect, TargetHand, 1.5f, false);
		}
		else
		{
			PC->PlayDynamicForceFeedback(1.5f, 0.05f, !bIsRight, bIsRight, !bIsRight, bIsRight, EDynamicForceFeedbackAction::Start);
		}
	}

	// --- AUDIO: ON LATCH ---
	if (ActiveHands.Num() == 1 && SoundOnLatch)
	{
		UGameplayStatics::SpawnSoundAttached(SoundOnLatch, GetOwner()->GetRootComponent());
	}

	// --- SMART GHOST HAND ASSIGNMENT ---
	AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner());

	USceneComponent* GhostHand = nullptr;
	if (bUseLatchSnapHand)
	{
		GhostHand = bIsRight ? CustomRightSnap : CustomLeftSnap;
	}
	else if (Parent)
	{
		GhostHand = bIsRight ? Parent->RightSnap : Parent->LeftSnap;
	}

	if (GhostHand)
	{
		GhostHand->SetAbsolute(false, false, true);
		GhostHand->SetHiddenInGame(false);
		GhostHand->SetVisibility(true);

		FTransform HandleWorld = TargetHandleMesh ? TargetHandleMesh->GetComponentTransform() : GetOwner()->GetActorTransform();
		HandFlyStartRelative = Hand->GetComponentTransform().GetRelativeTransform(HandleWorld);

		GhostHand->AttachToComponent(TargetHandleMesh, FAttachmentTransformRules::KeepRelativeTransform);
		GhostHand->SetRelativeTransform(HandFlyStartRelative);

		ActiveGhostHands.AddUnique(GhostHand);
		bIsHandFlying = true;
		HandFlyTime = 0.0f;
	}

	if (APawn* PlayerPawn = Cast<APawn>(Hand->GetOwner()))
	{
		if (AAzr_Pawn* AzrPawn = Cast<AAzr_Pawn>(PlayerPawn))
		{
			if (bIsRight && AzrPawn->RightHandMesh) AzrPawn->RightHandMesh->SetHiddenInGame(true);
			else if (!bIsRight && AzrPawn->LeftHandMesh) AzrPawn->LeftHandMesh->SetHiddenInGame(true);
		}
	}

	if (LinkedLatchZone && LinkedLatchZone->SpawnedLatchIndicator)
	{
		LinkedLatchZone->SpawnedLatchIndicator->SetActorHiddenInGame(true);
	}

	ToggleHighlight(false);
	ToggleTether(false);
	if (CurrentTargetWidget) CurrentTargetWidget->SetVisibility(false);
	UpdatePointer(true);

	// --- LEADER LOGIC ---
	if (ActiveHands.Num() == 1)
	{
		if (LatchType == EAzr_LatchType::Angular) LastAngle = CalculateAngle(Hand->GetComponentLocation());
		else if (LatchType == EAzr_LatchType::Rotation) LastAngle = CalculateTwistAngle(Hand);
		else GrabOffsetValue = CalculateLinearDist(Hand->GetComponentLocation()) - CurrentRawValue;
	}

	OnLatched.Broadcast();
}

void UAzr_Latch::ReleaseSpecificHand(USceneComponent* Hand)
{
	if (!Hand || !ActiveHands.Contains(Hand)) return;

	bool bWasLeader = (ActiveHands.Num() > 0 && ActiveHands[0] == Hand);

	if (bIsGrabEnabled)
	{
		ToggleHighlight(true);
		if (GetWorld()) { LastValue = 0.0f; bWasRising = true; }
		ToggleTether(true);
		if (CurrentTargetWidget) CurrentTargetWidget->SetVisibility(true);
		UpdatePointer(false);

		// --- THE FIX: Bring UI back when released ---
		if (LinkedLatchZone && LinkedLatchZone->SpawnedLatchIndicator)
		{
			LinkedLatchZone->SpawnedLatchIndicator->SetActorHiddenInGame(false);
		}
	}

	bool bIsRight = false;
	if (UAzr_HandScanner* Scanner = Cast<UAzr_HandScanner>(Hand)) bIsRight = Scanner->bIsRightHand;
	else bIsRight = Hand->ComponentHasTag(FName("Right"));

	// --- HAPTIC: SMART RELEASE (VR Native) ---
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		EControllerHand TargetHand = bIsRight ? EControllerHand::Right : EControllerHand::Left;
		float HapticScale = (ActiveHands.Num() == 1) ? 1.5f : 0.5f;

		if (HapticEffect)
		{
			PC->PlayHapticEffect(HapticEffect, TargetHand, HapticScale, false);
		}
		else
		{
			PC->PlayDynamicForceFeedback(HapticScale, 0.05f, !bIsRight, bIsRight, !bIsRight, bIsRight, EDynamicForceFeedbackAction::Start);
		}
	}

	if (APawn* PlayerPawn = Cast<APawn>(Hand->GetOwner()))
	{
		if (AAzr_Pawn* AzrPawn = Cast<AAzr_Pawn>(PlayerPawn))
		{
			if (bIsRight && AzrPawn->RightHandMesh) AzrPawn->RightHandMesh->SetHiddenInGame(false);
			else if (!bIsRight && AzrPawn->LeftHandMesh) AzrPawn->LeftHandMesh->SetHiddenInGame(false);
		}
	}

	// --- GHOST HAND CLEANUP ---
	AAzr_Interactable* Parent = Cast<AAzr_Interactable>(GetOwner());

	// Explicitly check the ecosystem split boolean
	USceneComponent* GhostHand = nullptr;
	if (bUseLatchSnapHand)
	{
		GhostHand = bIsRight ? CustomRightSnap : CustomLeftSnap;
	}
	else if (Parent)
	{
		GhostHand = bIsRight ? Parent->RightSnap : Parent->LeftSnap;
	}

	if (GhostHand)
	{
		GhostHand->SetHiddenInGame(true);
		GhostHand->SetVisibility(false);
		ActiveGhostHands.Remove(GhostHand);
	}

	ActiveHands.Remove(Hand);

	if (ActiveHands.Num() == 0)
	{
		bIsGrabbed = false;
		bIsHandFlying = false;
		bIsTriggerEngaged = false;

		if (SoundOnRelease) UGameplayStatics::SpawnSoundAttached(SoundOnRelease, GetOwner()->GetRootComponent());

		if (bIsGrabEnabled)
		{
			ToggleHighlight(true);
			if (GetWorld()) { LastValue = 0.0f; bWasRising = true; }
			ToggleTether(true);
			if (CurrentTargetWidget) CurrentTargetWidget->SetVisibility(true);
			UpdatePointer(false);
		}
		else
		{
			
			SetComponentTickEnabled(false);
		}

		OnLatchReleased.Broadcast();
	}
	else if (bWasLeader)
	{
	
		USceneComponent* NewLeader = ActiveHands[0];
		if (LatchType == EAzr_LatchType::Angular) LastAngle = CalculateAngle(NewLeader->GetComponentLocation());
		else if (LatchType == EAzr_LatchType::Rotation) LastAngle = CalculateTwistAngle(NewLeader);
		else GrabOffsetValue = CalculateLinearDist(NewLeader->GetComponentLocation()) - CurrentRawValue;
	}
}

void UAzr_Latch::ReleaseLatch()
{
	if (ActiveHands.Num() > 0)
	{
		ReleaseSpecificHand(ActiveHands[0]);
	}
}

void UAzr_Latch::CheckBreakDistance()
{
	if (MaxBreakDistance <= 0.0f) return;

	for (int32 i = ActiveHands.Num() - 1; i >= 0; i--)
	{
		USceneComponent* Hand = ActiveHands[i];
		USceneComponent* TargetComp = TargetHandleMesh ? Cast<USceneComponent>(TargetHandleMesh) : this;

		if (FVector::DistSquared(Hand->GetComponentLocation(), TargetComp->GetComponentLocation()) > (MaxBreakDistance * MaxBreakDistance))
		{
			ReleaseSpecificHand(Hand);

			if (APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
			{
				if (HapticEffect)
				{
					bool bIsRight = false;
					if (UAzr_HandScanner* Scanner = Cast<UAzr_HandScanner>(Hand)) bIsRight = Scanner->bIsRightHand;
					else bIsRight = Hand->ComponentHasTag(FName("Right"));

					PC->PlayHapticEffect(HapticEffect, bIsRight ? EControllerHand::Right : EControllerHand::Left, 1.5f, false);
				}
				else
				{
					PC->PlayDynamicForceFeedback(1.5f, 0.1f, true, true, true, true, EDynamicForceFeedbackAction::Start);
				}
			}
		}
	}
}

// --- PHYSICS ENGINE ---

void UAzr_Latch::HandleAngularInteraction(float DeltaTime)
{
	FVector HandPos = ActiveHands[0]->GetComponentLocation();

	// --- MINIMUM LEVER ARM (Anti-Jitter) ---
	FVector PivotPos = GetComponentLocation();
	FVector Axis = GetAxisVector();
	FVector ToHand = HandPos - PivotPos;
	FVector ProjectedHand = ToHand - (Axis * FVector::DotProduct(ToHand, Axis));

	if (ProjectedHand.Size() < MinLeverArmLength)
	{
		LastAngle = CalculateAngle(HandPos);
		return;
	}

	float PrePhysicsValue = CurrentRawValue;
	float CurrentAngle = CalculateAngle(HandPos);
	float DeltaAngle = FMath::FindDeltaAngleDegrees(LastAngle, CurrentAngle);
	LastAngle = CurrentAngle;

	if (bInvertRotation) DeltaAngle *= -1.0f;

	CurrentRawValue += DeltaAngle;
	ApplyConstraints(PrePhysicsValue);
}

void UAzr_Latch::HandleLinearInteraction(float DeltaTime)
{
	float PrePhysicsValue = CurrentRawValue;
	float HandDist = CalculateLinearDist(ActiveHands[0]->GetComponentLocation());
	float TargetValue = HandDist - GrabOffsetValue;
	CurrentRawValue = FMath::FInterpTo(CurrentRawValue, TargetValue, DeltaTime, Damping);

	ApplyConstraints(PrePhysicsValue);
}

void UAzr_Latch::HandleRotationInteraction(float DeltaTime)
{
	float PrePhysicsValue = CurrentRawValue;
	USceneComponent* LeaderHand = ActiveHands[0];

	// --- DYNAMIC VECTOR TRACKING ---
	FTransform ParentWorld = GetAttachParent() ? GetAttachParent()->GetComponentTransform() : FTransform::Identity;
	FTransform ZeroTransform = InitialTransform * ParentWorld;
	FTransform RealHandLocal = LeaderHand->GetComponentTransform().GetRelativeTransform(ZeroTransform);

	FVector UpVec = RealHandLocal.TransformVectorNoScale(FVector::UpVector);
	FVector InteractionAxisVec = GetAxisVector();
	float UpDot = FMath::Abs(FVector::DotProduct(UpVec, InteractionAxisVec));
	FVector SafestAxis = (UpDot > 0.7f) ? FVector::RightVector : FVector::UpVector;

	if (TrackedLocalHandAxis != SafestAxis)
	{
		TrackedLocalHandAxis = SafestAxis;
		LastAngle = CalculateTwistAngle(LeaderHand);
	}

	// --- MECHANICAL DAMPING ---
	float CurrentAngle = CalculateTwistAngle(LeaderHand);
	float RawDelta = FMath::FindDeltaAngleDegrees(LastAngle, CurrentAngle);
	float SmoothedDelta = RawDelta * FMath::Clamp(DeltaTime * Damping, 0.0f, 1.0f);

	LastAngle += SmoothedDelta;
	LastAngle = FRotator::NormalizeAxis(LastAngle);

	if (bInvertRotation) SmoothedDelta *= -1.0f;

	CurrentRawValue += SmoothedDelta;
	ApplyConstraints(PrePhysicsValue);
}

void UAzr_Latch::HandleAutoReturn(float DeltaTime)
{
	float PrePhysicsValue = CurrentRawValue;
	CurrentRawValue = FMath::FInterpTo(CurrentRawValue, ReturnValue, DeltaTime, ReturnSpeed);
	ApplyConstraints(PrePhysicsValue);

	if (FMath::IsNearlyEqual(CurrentRawValue, ReturnValue, 0.05f))
	{
		float SnapPre = CurrentRawValue;
		CurrentRawValue = ReturnValue;
		ApplyConstraints(SnapPre);

		if (ActiveHands.Num() == 0 && HighlightMode == EAzr_HighlightMode::None)
			SetComponentTickEnabled(false);
	}
}

void UAzr_Latch::ApplyConstraints(float PrePhysicsRawValue)
{
	CurrentRawValue = FMath::Clamp(CurrentRawValue, LimitMin, LimitMax);

	if (LatchType == EAzr_LatchType::Angular || LatchType == EAzr_LatchType::Rotation)
	{
		FRotator OffsetRot = FRotator::ZeroRotator;
		switch (InteractionAxis)
		{
		case EAzr_Axis::X_Axis: OffsetRot.Roll = CurrentRawValue; break;
		case EAzr_Axis::Y_Axis: OffsetRot.Pitch = CurrentRawValue; break;
		case EAzr_Axis::Z_Axis: OffsetRot.Yaw = CurrentRawValue; break;
		}
		SetRelativeRotation(InitialTransform.GetRotation() * OffsetRot.Quaternion());
	}
	else
	{
		FVector NewLocation = InitialTransform.GetLocation();

		// Override ONLY the target axis with the absolute StartValue/CurrentRawValue
		switch (InteractionAxis)
		{
		case EAzr_Axis::X_Axis: NewLocation.X = CurrentRawValue; break;
		case EAzr_Axis::Y_Axis: NewLocation.Y = CurrentRawValue; break;
		case EAzr_Axis::Z_Axis: NewLocation.Z = CurrentRawValue; break;
		}

		SetRelativeLocation(NewLocation);
	}

	if (!FMath::IsNearlyEqual(PrePhysicsRawValue, CurrentRawValue, 0.001f))
	{
		OnRawValueChanged.Broadcast(CurrentRawValue);
		float Norm = (LimitMax - LimitMin) != 0.0f ? (CurrentRawValue - LimitMin) / (LimitMax - LimitMin) : 0.0f;
		OnValueChanged.Broadcast(Norm);

		if (bHapticOnStep && FMath::Abs(CurrentRawValue - LastHapticValue) >= StepSize)
		{
			for (USceneComponent* Hand : ActiveHands)
			{
				APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
				UAzr_HandScanner* Scanner = Cast<UAzr_HandScanner>(Hand);
				if (PC && Scanner)
				{
					EControllerHand TargetHand = Scanner->bIsRightHand ? EControllerHand::Right : EControllerHand::Left;
					if (HapticEffect) PC->PlayHapticEffect(HapticEffect, TargetHand, HapticIntensity, false);
				}
			}
			LastHapticValue = CurrentRawValue;
		}
	}
}

// --- LIFECYCLE & HELPERS ---

void UAzr_Latch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsGrabEnabled) DisableLatch();
	Super::EndPlay(EndPlayReason);
}

void UAzr_Latch::EnsureInitialized()
{
	if (!GetOwner()) return;
	if (StartAnchor->GetAttachParent() == nullptr)
	{
		StartAnchor->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		EndAnchor->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		TetherCable->AttachToComponent(StartAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	if (!TargetHandleMesh) TargetHandleMesh = FindMeshByName(HandleMeshName);
	if (!AutoDetectedMesh)
	{
		if (TargetHandleMesh) AutoDetectedMesh = TargetHandleMesh;
		else
		{
			TArray<UStaticMeshComponent*> MeshComps;
			GetOwner()->GetComponents<UStaticMeshComponent>(MeshComps);
			for (UStaticMeshComponent* Mesh : MeshComps)
			{
				if (Mesh && Mesh != StartAnchor && Mesh != EndAnchor)
				{
					AutoDetectedMesh = Mesh;
					break;
				}
			}
		}
	}
}

void UAzr_Latch::AutoDetectLatchZone()
{
	if (!GetOwner()) return;

	TArray<UAzr_LatchZone*> FoundZones;
	GetOwner()->GetComponents<UAzr_LatchZone>(FoundZones);

	// --- UPDATED: The Modular Matchmaker ---
	for (UAzr_LatchZone* Zone : FoundZones)
	{
		if (Zone && Zone->InteractID == this->InteractID)
		{
			LinkedLatchZone = Zone;
			break; // Found our exact match!
		}
	}
}

// --- VISUAL HELPERS ---

void UAzr_Latch::UpdatePointer(bool bIsGrabbing)
{
	UAzr_Pointer* Pointer = FindPlayerPointer();
	if (!Pointer) return;
	USceneComponent* Target = TargetHandleMesh ? Cast<USceneComponent>(TargetHandleMesh) : AutoDetectedMesh;
	if (!Target) Target = this;
	Pointer->EnablePointer_TargetComponent(Target);
}

void UAzr_Latch::ToggleTether(bool bState)
{
	if (!bState || !TetherSettings.bEnableTether)
	{
		StartAnchor->SetVisibility(false); EndAnchor->SetVisibility(false); TetherCable->SetVisibility(false); return;
	}

	USceneComponent* MeshTarget = TargetHandleMesh ? Cast<USceneComponent>(TargetHandleMesh) : AutoDetectedMesh;
	USceneComponent* WidgetTarget = FindWidgetByName(TetherSettings.TargetWidgetName);
	if (!MeshTarget || !WidgetTarget) return;

	if (TetherSettings.AnchorMesh) { StartAnchor->SetStaticMesh(TetherSettings.AnchorMesh); EndAnchor->SetStaticMesh(TetherSettings.AnchorMesh); }
	StartAnchor->SetWorldScale3D(FVector(TetherSettings.AnchorScale));
	EndAnchor->SetWorldScale3D(FVector(TetherSettings.AnchorScale));
	if (TetherSettings.CableMaterial) TetherCable->SetMaterial(0, TetherSettings.CableMaterial);
	TetherCable->CableWidth = TetherSettings.CableWidth;

	// --- DRIFT FIX: Lazy Attachment Check ---
	bool bStartCorrect = (StartAnchor->GetAttachParent() == MeshTarget);
	bool bEndCorrect = (EndAnchor->GetAttachParent() == WidgetTarget);

	if (!bStartCorrect || !bEndCorrect)
	{
		FVector StartPos = CalculateSurfaceAnchor(MeshTarget, TetherSettings.MeshAnchorPos, TetherSettings);
		FVector EndPos = CalculateSurfaceAnchor(WidgetTarget, TetherSettings.WidgetAnchorPos, TetherSettings);

		if (!bStartCorrect) StartAnchor->AttachToComponent(MeshTarget, FAttachmentTransformRules::KeepWorldTransform);
		if (!bEndCorrect) EndAnchor->AttachToComponent(WidgetTarget, FAttachmentTransformRules::KeepWorldTransform);

		StartAnchor->SetWorldLocation(StartPos);
		EndAnchor->SetWorldLocation(EndPos);
	}

	TetherCable->SetAttachEndToComponent(EndAnchor);
	TetherCable->SetRelativeLocation(FVector::ZeroVector);
	TetherCable->EndLocation = FVector::ZeroVector;

	// --- NUKE ALL COLLISION ---
	TetherCable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TetherCable->SetCollisionResponseToAllChannels(ECR_Ignore);
	TetherCable->bEnableCollision = false;

	// --- PERCENTAGE MATH & STIFF HANG ---
	float HangPercentage = TetherSettings.CableHang / 100.0f;
	float InitialDist = FVector::Dist(StartAnchor->GetComponentLocation(), EndAnchor->GetComponentLocation());

	if (HangPercentage <= 0.001f) {
		TetherCable->NumSegments = 1;
		TetherCable->CableLength = InitialDist;
		TetherCable->CableGravityScale = 0.0f;
		TetherCable->bEnableStiffness = false;
	}
	else {
		TetherCable->NumSegments = 20;
		float SlackAmount = InitialDist * HangPercentage;
		TetherCable->CableLength = InitialDist + SlackAmount;
		TetherCable->CableGravityScale = FMath::Clamp(HangPercentage * 0.5f, 0.01f, 0.5f);
		TetherCable->bEnableStiffness = true;
		TetherCable->SolverIterations = 16;
	}

	TetherCable->RecreatePhysicsState();

	StartAnchor->SetVisibility(true);
	EndAnchor->SetVisibility(true);

	// --- THE FIRST-TIME ONLY TIMER LOGIC ---
	if (!bHasTetherSettled)
	{
		// First time ever: Hide it and wait 0.2s for physics to settle
		bHasTetherSettled = true;
		TetherCable->SetVisibility(false);

		if (UWorld* World = GetWorld()) {
			FTimerHandle SettleTimer;
			World->GetTimerManager().SetTimer(SettleTimer, FTimerDelegate::CreateWeakLambda(this, [this]() {
				// Only show if the player hasn't already latched during this 0.2s window
				if (TetherSettings.bEnableTether && TetherCable) {
					TetherCable->SetVisibility(true);
				}
				}), 0.20f, false);
		}
	}
	else
	{
		// Every other time (Unhiding after latch release): Show instantly
		TetherCable->SetVisibility(true);
	}
}


void UAzr_Latch::ToggleHighlight(bool bState)
{
	if (HighlightMode == EAzr_HighlightMode::None) return;
	TArray<UMeshComponent*> Meshes;
	if (HighlightMode == EAzr_HighlightMode::AllComponents) GetOwner()->GetComponents(Meshes);
	else if (TargetHandleMesh) Meshes.Add(Cast<UMeshComponent>(TargetHandleMesh));
	else if (AutoDetectedMesh) Meshes.Add(Cast<UMeshComponent>(AutoDetectedMesh));

	for (UMeshComponent* Mesh : Meshes) if (Mesh) { Mesh->SetRenderCustomDepth(bState); Mesh->SetCustomDepthStencilValue(StencilID); }
}

// --- MATH & UTILS ---

float UAzr_Latch::CalculateAngle(FVector HandPos) const
{
	FTransform ParentWorld = GetAttachParent() ? GetAttachParent()->GetComponentTransform() : FTransform::Identity;
	FTransform ZeroTransform = InitialTransform * ParentWorld;
	FVector LocalPoint = ZeroTransform.InverseTransformPosition(HandPos);
	switch (InteractionAxis)
	{
	case EAzr_Axis::X_Axis: return -UKismetMathLibrary::DegAtan2(LocalPoint.Z, LocalPoint.Y);
	case EAzr_Axis::Y_Axis: return UKismetMathLibrary::DegAtan2(LocalPoint.Z, LocalPoint.X);
	case EAzr_Axis::Z_Axis: return UKismetMathLibrary::DegAtan2(LocalPoint.Y, LocalPoint.X);
	default: return 0.0f;
	}
}

float UAzr_Latch::CalculateLinearDist(FVector HandPos) const
{
	FTransform ParentWorld = GetAttachParent() ? GetAttachParent()->GetComponentTransform() : FTransform::Identity;
	FTransform ZeroTransform = InitialTransform * ParentWorld;
	FVector LocalPoint = ZeroTransform.InverseTransformPosition(HandPos);
	return FVector::DotProduct(LocalPoint, GetAxisVector());
}

float UAzr_Latch::CalculateTwistAngle(USceneComponent* Hand) const
{
	if (!Hand) return 0.0f;
	FTransform ParentWorld = GetAttachParent() ? GetAttachParent()->GetComponentTransform() : FTransform::Identity;
	FTransform ZeroTransform = InitialTransform * ParentWorld;
	FTransform RealHandLocal = Hand->GetComponentTransform().GetRelativeTransform(ZeroTransform);
	FVector TrackedVector = RealHandLocal.TransformVectorNoScale(TrackedLocalHandAxis);
	switch (InteractionAxis)
	{
	case EAzr_Axis::X_Axis: return -UKismetMathLibrary::DegAtan2(TrackedVector.Z, TrackedVector.Y);
	case EAzr_Axis::Y_Axis: return UKismetMathLibrary::DegAtan2(TrackedVector.Z, TrackedVector.X);
	case EAzr_Axis::Z_Axis: return UKismetMathLibrary::DegAtan2(TrackedVector.Y, TrackedVector.X);
	default: return 0.0f;
	}
}

FVector UAzr_Latch::GetAxisVector() const
{
	switch (InteractionAxis)
	{
	case EAzr_Axis::X_Axis: return FVector::ForwardVector;
	case EAzr_Axis::Y_Axis: return FVector::RightVector;
	case EAzr_Axis::Z_Axis: return FVector::UpVector;
	default: return FVector::ForwardVector;
	}
}

FVector UAzr_Latch::CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos, const FAzr_TetherConfig& Config)
{
	if (!Target) return FVector::ZeroVector;

	// --- WIDGET LOGIC ---
	if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(Target))
	{
		FVector2D DrawSize = WidgetComp->GetCurrentDrawSize();
		FVector Scale = WidgetComp->GetComponentScale();
		FVector2D Pivot = WidgetComp->GetPivot();

		float WorldHalfWidth = (DrawSize.X * 0.5f) * Scale.Y;
		float WorldHalfHeight = (DrawSize.Y * 0.5f) * Scale.Z;

		FVector Center = WidgetComp->GetComponentLocation();
		FVector RightVec = WidgetComp->GetRightVector();
		FVector UpVec = WidgetComp->GetUpVector();
		FVector ForwardVec = WidgetComp->GetForwardVector();

		float PivotShiftX = (0.5f - Pivot.X) * (DrawSize.X * Scale.Y);
		float PivotShiftY = (0.5f - Pivot.Y) * (DrawSize.Y * Scale.Z);
		FVector VisualCenter = Center + (RightVec * PivotShiftX) + (UpVec * PivotShiftY);

		switch (Pos)
		{
		case EAzr_TetherPos::Top: return VisualCenter + (UpVec * (WorldHalfHeight + Config.WidgetGap_Vertical));
		case EAzr_TetherPos::Bottom: return VisualCenter - (UpVec * (WorldHalfHeight + Config.WidgetGap_Vertical));
		case EAzr_TetherPos::Right: return VisualCenter + (RightVec * (WorldHalfWidth + Config.WidgetGap_Horizontal));
		case EAzr_TetherPos::Left: return VisualCenter - (RightVec * (WorldHalfWidth + Config.WidgetGap_Horizontal));
		case EAzr_TetherPos::Front: return VisualCenter + (ForwardVec * 1.0f);
		case EAzr_TetherPos::Back: return VisualCenter - (ForwardVec * 1.0f);
		default: return VisualCenter;
		}
	}

	// --- MESH LOGIC ---
	FBoxSphereBounds LocalBounds = Target->CalcLocalBounds();
	FVector WorldCenter = Target->GetComponentTransform().TransformPosition(LocalBounds.Origin);
	FVector ForwardVec = Target->GetForwardVector();
	FVector RightVec = Target->GetRightVector();
	FVector UpVec = Target->GetUpVector();
	FVector Scale = Target->GetComponentScale();

	float WorldHalfDepth = LocalBounds.BoxExtent.X * FMath::Abs(Scale.X);
	float WorldHalfWidth = LocalBounds.BoxExtent.Y * FMath::Abs(Scale.Y);
	float WorldHalfHeight = LocalBounds.BoxExtent.Z * FMath::Abs(Scale.Z);

	FVector SurfacePush = FVector::ZeroVector;

	switch (Pos)
	{
	case EAzr_TetherPos::Top: SurfacePush = UpVec * (WorldHalfHeight + Config.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Bottom: SurfacePush = -UpVec * (WorldHalfHeight + Config.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Right: SurfacePush = RightVec * (WorldHalfWidth + Config.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Left: SurfacePush = -RightVec * (WorldHalfWidth + Config.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Front: SurfacePush = ForwardVec * (WorldHalfDepth + Config.MeshSurfaceOffset); break;
	case EAzr_TetherPos::Back: SurfacePush = -ForwardVec * (WorldHalfDepth + Config.MeshSurfaceOffset); break;
	default: break;
	}

	return WorldCenter + SurfacePush + (UpVec * Config.MeshOffset_Vertical) + (RightVec * Config.MeshOffset_Horizontal);
}

UPrimitiveComponent* UAzr_Latch::FindMeshByName(FName Name)
{
	if (Name.IsNone()) return nullptr;
	TArray<UPrimitiveComponent*> Comps; GetOwner()->GetComponents(Comps);
	for (UPrimitiveComponent* Comp : Comps) if (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString())) return Comp;
	return nullptr;
}

USceneComponent* UAzr_Latch::FindWidgetByName(FName Name)
{
	TArray<USceneComponent*> Comps; GetOwner()->GetComponents(Comps);
	for (USceneComponent* Comp : Comps) if (Comp->IsA(UWidgetComponent::StaticClass()) && (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString()))) return Cast<USceneComponent>(Comp);
	return nullptr;
}

UAzr_Pointer* UAzr_Latch::FindPlayerPointer() const
{
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0)) return PlayerPawn->FindComponentByClass<UAzr_Pointer>();
	return nullptr;
}

void UAzr_Latch::ForceLatchRelease()
{
	// Safely iterates through every hand currently holding the latch and forces them to drop it
	while (ActiveHands.Num() > 0)
	{
		ReleaseSpecificHand(ActiveHands[0]);
	}
}