

#include "Azr_Pawn.h"
#include "Azr_ExplainWidget.h"
#include "Azr_Gaze.h"
#include "Azr_Grab.h"
#include "Azr_HandAnimInstance.h"
#include "Azr_Latch.h"
#include "Azr_Teleport.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "NavigationSystem.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IXRTrackingSystem.h"
#include "TimerManager.h"

AAzr_Pawn::AAzr_Pawn() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;

    VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
    RootComponent = VROrigin;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(VROrigin);

    // --- FACE-LOCKED RETICLE SETUP ---
    GazeReticleWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("GazeReticleWidget"));
    GazeReticleWidget->SetupAttachment(Camera);
    GazeReticleWidget->SetRelativeLocation(FVector(100.0f, 0.0f, 0.0f));
    GazeReticleWidget->SetWidgetSpace(EWidgetSpace::World);
    GazeReticleWidget->SetDrawSize(FVector2D(100.0f, 100.0f));
    GazeReticleWidget->SetVisibility(false);
    GazeReticleWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // --- CONTROLLERS ---
    LeftMotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftController"));
    LeftMotionController->SetupAttachment(VROrigin);
    LeftMotionController->SetTrackingMotionSource(FName("Left"));

    RightMotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightController"));
    RightMotionController->SetupAttachment(VROrigin);
    RightMotionController->SetTrackingMotionSource(FName("Right"));

    // --- HAND MESHES ---
    LeftHandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LeftHandMesh"));
    LeftHandMesh->SetupAttachment(LeftMotionController);

    RightHandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RightHandMesh"));
    RightHandMesh->SetupAttachment(RightMotionController);

    // --- SCANNERS ---
    LeftHandScanner = CreateDefaultSubobject<UAzr_HandScanner>(TEXT("LeftHandScanner"));
    LeftHandScanner->SetupAttachment(LeftMotionController);
    LeftHandScanner->bIsRightHand = false;
    LeftHandScanner->InteractProfile = FName("Azr_Collision");

    RightHandScanner = CreateDefaultSubobject<UAzr_HandScanner>(TEXT("RightHandScanner"));
    RightHandScanner->SetupAttachment(RightMotionController);
    RightHandScanner->bIsRightHand = true;
    RightHandScanner->InteractProfile = FName("Azr_Collision");

    // --- UI INTERACTION (SMART LASERS) ---
    MaxLaserDistance = 1500.0f;
    LaserTraceChannel = ECC_Visibility;

    // Left UI Setup
    LeftWidgetInteraction = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("LeftWidgetInteraction"));
    LeftWidgetInteraction->SetupAttachment(LeftMotionController);
    LeftWidgetInteraction->InteractionSource = EWidgetInteractionSource::World;
    LeftWidgetInteraction->bShowDebug = false;
    LeftWidgetInteraction->PointerIndex = 1;

    LeftLaserBeam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLaserBeam"));
    LeftLaserBeam->SetupAttachment(LeftMotionController);
    LeftLaserBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftLaserBeam->SetCastShadow(false);
    LeftLaserBeam->SetVisibility(false);
    LeftLaserBeam->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

    LeftLaserReticle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLaserReticle"));
    LeftLaserReticle->SetupAttachment(LeftMotionController);
    LeftLaserReticle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftLaserReticle->SetCastShadow(false);
    LeftLaserReticle->SetVisibility(false);

    // Right UI Setup
    RightWidgetInteraction = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("RightWidgetInteraction"));
    RightWidgetInteraction->SetupAttachment(RightMotionController);
    RightWidgetInteraction->InteractionSource = EWidgetInteractionSource::World;
    RightWidgetInteraction->bShowDebug = false;
    RightWidgetInteraction->PointerIndex = 2;

    RightLaserBeam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightLaserBeam"));
    RightLaserBeam->SetupAttachment(RightMotionController);
    RightLaserBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightLaserBeam->SetCastShadow(false);
    RightLaserBeam->SetVisibility(false);
    RightLaserBeam->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

    RightLaserReticle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightLaserReticle"));
    RightLaserReticle->SetupAttachment(RightMotionController);
    RightLaserReticle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightLaserReticle->SetCastShadow(false);
    RightLaserReticle->SetVisibility(false);

    // --- DEFAULTS ---
    bIsVRMode = false;
    HandAnimSpeed = 15.0f;

    // Default to Standard (Right Hand Turn/Teleport, Left Hand Move)
    DominantLocomotionHand = EAzr_LocomotionHand::RightHand;
    ForwardInput = EForwardBehavior::Teleport;
    TurnInput = ETurnBehavior::SnapTurn;
    BackwardInput = EBackBehavior::BlinkStep;
    SnapTurnAngle = 45.0f;
    SmoothTurnSpeed = 90.0f;
    SmoothMoveSpeed = 300.0f;
    BlinkStepDistance = 150.0f;
    bReadyToSnapTurn = true;
    GazeTraceDistance = 500.0f;
    GazeTraceRadius = 5.0f;
    bShowGazeDebug = false;

    bLeftWasHoveringWidget = false;
    bRightWasHoveringWidget = false;

    // Initialize Memory Pointers
    LastHoveredLeftWidget = nullptr;
    LastHoveredRightWidget = nullptr;
}

void AAzr_Pawn::BeginPlay() {
    Super::BeginPlay();
    DetectPlatform();

    CachedTeleportComp = FindComponentByClass<UAzr_Teleport>();

    if (LeftHandMesh) LeftHandAnim = Cast<UAzr_HandAnimInstance>(LeftHandMesh->GetAnimInstance());
    if (RightHandMesh) RightHandAnim = Cast<UAzr_HandAnimInstance>(RightHandMesh->GetAnimInstance());

    LeftWidgetInteraction->InteractionDistance = MaxLaserDistance;
    LeftWidgetInteraction->TraceChannel = LaserTraceChannel;
    RightWidgetInteraction->InteractionDistance = MaxLaserDistance;
    RightWidgetInteraction->TraceChannel = LaserTraceChannel;

    FVector StartLoc = GetActorLocation();
    FVector EndLoc = StartLoc - FVector(0, 0, 250.0f);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    if (GetWorld()->LineTraceSingleByChannel(Hit, StartLoc, EndLoc, ECC_WorldStatic, Params)) {
        if (Hit.Distance > 5.0f) SetActorLocation(Hit.Location);
    }
}

void AAzr_Pawn::DetectPlatform() {
    if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled()) {
        bIsVRMode = true;
        UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Stage);
    }
    else {
        bIsVRMode = false;
        Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 170.0f));
    }
}

void AAzr_Pawn::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    ProcessGazeTrace();
    UpdateSmartLasers();

    if (!FMath::IsNearlyEqual(CurrentGripLeft, TargetGripLeft, 0.001f)) {
        CurrentGripLeft = FMath::FInterpTo(CurrentGripLeft, TargetGripLeft, DeltaTime, HandAnimSpeed);
        if (!LeftHandAnim && LeftHandMesh) LeftHandAnim = Cast<UAzr_HandAnimInstance>(LeftHandMesh->GetAnimInstance());
        if (LeftHandAnim) LeftHandAnim->GripAlpha = CurrentGripLeft;
    }

    if (!FMath::IsNearlyEqual(CurrentGripRight, TargetGripRight, 0.001f)) {
        CurrentGripRight = FMath::FInterpTo(CurrentGripRight, TargetGripRight, DeltaTime, HandAnimSpeed);
        if (!RightHandAnim && RightHandMesh) RightHandAnim = Cast<UAzr_HandAnimInstance>(RightHandMesh->GetAnimInstance());
        if (RightHandAnim) RightHandAnim->GripAlpha = CurrentGripRight;
    }
}

void AAzr_Pawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (APlayerController* PC = Cast<APlayerController>(GetController())) {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())) {
            if (DefaultMappingContext) Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
        // THE FIX: Bound "Completed" so left hand can properly finish teleports
        if (IA_Move) {
            EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AAzr_Pawn::Move);
            EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Completed, this, &AAzr_Pawn::Move);
        }

        if (IA_Look) {
            EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AAzr_Pawn::RightStickInput);
            EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Completed, this, &AAzr_Pawn::RightStickInput);
            EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AAzr_Pawn::Look);
        }

        if (IA_Grab_Left) {
            EnhancedInputComponent->BindAction(IA_Grab_Left, ETriggerEvent::Triggered, this, &AAzr_Pawn::GrabLeft);
            EnhancedInputComponent->BindAction(IA_Grab_Left, ETriggerEvent::Completed, this, &AAzr_Pawn::ReleaseLeft);
        }
        if (IA_Grab_Right) {
            EnhancedInputComponent->BindAction(IA_Grab_Right, ETriggerEvent::Triggered, this, &AAzr_Pawn::GrabRight);
            EnhancedInputComponent->BindAction(IA_Grab_Right, ETriggerEvent::Completed, this, &AAzr_Pawn::ReleaseRight);
        }

        if (IA_Trigger_Left) {
            EnhancedInputComponent->BindAction(IA_Trigger_Left, ETriggerEvent::Triggered, this, &AAzr_Pawn::OnTriggerLeft);
            EnhancedInputComponent->BindAction(IA_Trigger_Left, ETriggerEvent::Completed, this, &AAzr_Pawn::OnTriggerLeft);
        }
        if (IA_Trigger_Right) {
            EnhancedInputComponent->BindAction(IA_Trigger_Right, ETriggerEvent::Triggered, this, &AAzr_Pawn::OnTriggerRight);
            EnhancedInputComponent->BindAction(IA_Trigger_Right, ETriggerEvent::Completed, this, &AAzr_Pawn::OnTriggerRight);
        }
    }
}

void AAzr_Pawn::UpdateSmartLasers() {
    APlayerController* PC = Cast<APlayerController>(GetController());

    auto ProcessLaser = [PC](UWidgetInteractionComponent* WIC,
        UStaticMeshComponent* Beam,
        UStaticMeshComponent* Reticle, bool& bWasHovering,
        EControllerHand Hand,
        UAzr_ExplainWidget*& LastHoveredWidget) {

            if (WIC && WIC->IsOverHitTestVisibleWidget()) {
                Beam->SetVisibility(true);
                Reticle->SetVisibility(true);

                FHitResult Hit = WIC->GetLastHitResult();

                // --- 1. DYNAMIC LASER SCALING ---
                float ExactDistance = FVector::Dist(Beam->GetComponentLocation(), Hit.ImpactPoint);

                // DYNAMIC GAP LOGIC
                float DesiredGap = 400.0f;
                float DynamicGap = FMath::Min(DesiredGap, ExactDistance * 0.5f);
                float VisualBeamLength = FMath::Max(1.0f, ExactDistance - DynamicGap);

                float MeshHeight = 100.0f;
                if (Beam->GetStaticMesh()) {
                    MeshHeight = Beam->GetStaticMesh()->GetBoundingBox().GetSize().Z;
                }
                if (MeshHeight <= 0.0f) MeshHeight = 100.0f;

                FVector CurrentScale = Beam->GetRelativeScale3D();
                Beam->SetRelativeScale3D(FVector(CurrentScale.X, CurrentScale.Y, VisualBeamLength / MeshHeight));

                // --- 2. FIX RETICLE DELAY & CLIPPING ---
                FVector SafeHitPoint = Hit.ImpactPoint + (Hit.ImpactNormal * 1.5f);
                FVector LocalHitPoint = Reticle->GetAttachParent()->GetComponentTransform().InverseTransformPosition(SafeHitPoint);
                Reticle->SetRelativeLocation(LocalHitPoint);
                Reticle->SetWorldRotation(Hit.ImpactNormal.Rotation());

                // --- 3. AIM ON HAPTIC PULSE ---
                if (!bWasHovering) {
                    if (PC) {
                        PC->SetHapticsByValue(1.0f, 1.0f, Hand);
                        if (UWorld* World = Beam->GetWorld()) {
                            FTimerHandle HapticTimer;
                            World->GetTimerManager().SetTimer(HapticTimer, FTimerDelegate::CreateWeakLambda(PC, [PC, Hand]() {
                                if (PC) PC->SetHapticsByValue(0.0f, 0.0f, Hand);
                                }), 0.1f, false);
                        }
                    }
                }

                // --- 4. WIDGET HOVER LOGIC ---
                UAzr_ExplainWidget* HoveredExplainWidget = nullptr;
                if (UWidgetComponent* HitWidgetComp = Cast<UWidgetComponent>(Hit.GetComponent())) {
                    HoveredExplainWidget = Cast<UAzr_ExplainWidget>(HitWidgetComp->GetWidget());
                }

                if (HoveredExplainWidget != LastHoveredWidget) {
                    if (LastHoveredWidget) {
                        if (Hand == EControllerHand::Left) LastHoveredWidget->SetHoveredByLeftHand(false);
                        else LastHoveredWidget->SetHoveredByRightHand(false);
                    }
                    if (HoveredExplainWidget) {
                        if (Hand == EControllerHand::Left) HoveredExplainWidget->SetHoveredByLeftHand(true);
                        else HoveredExplainWidget->SetHoveredByRightHand(true);
                    }
                    LastHoveredWidget = HoveredExplainWidget;
                }

                bWasHovering = true;
            }
            else {
                Beam->SetVisibility(false);
                Reticle->SetVisibility(false);

                if (bWasHovering) {

                    // --- 5. AIM OFF HAPTIC PULSE ---
                    if (PC) {
                        PC->SetHapticsByValue(1.0f, 1.0f, Hand);
                        if (UWorld* World = Beam->GetWorld()) {
                            FTimerHandle HapticTimer;
                            World->GetTimerManager().SetTimer(HapticTimer, FTimerDelegate::CreateWeakLambda(PC, [PC, Hand]() {
                                if (PC) PC->SetHapticsByValue(0.0f, 0.0f, Hand);
                                }), 0.1f, false);
                        }
                    }

                    // --- MEMORY CLEANUP ---
                    if (LastHoveredWidget) {
                        if (Hand == EControllerHand::Left) LastHoveredWidget->SetHoveredByLeftHand(false);
                        else LastHoveredWidget->SetHoveredByRightHand(false);
                        LastHoveredWidget = nullptr;
                    }
                }

                bWasHovering = false;
            }
        };

    ProcessLaser(LeftWidgetInteraction, LeftLaserBeam, LeftLaserReticle,
        bLeftWasHoveringWidget, EControllerHand::Left, LastHoveredLeftWidget);
    ProcessLaser(RightWidgetInteraction, RightLaserBeam, RightLaserReticle,
        bRightWasHoveringWidget, EControllerHand::Right, LastHoveredRightWidget);
}

void AAzr_Pawn::OnTriggerLeft(const FInputActionValue& Value) {
    float AxisVal = Value.Get<float>();
    if (AxisVal > 0.5f) {
        if (LeftWidgetInteraction->IsOverHitTestVisibleWidget())
            LeftWidgetInteraction->PressPointerKey(EKeys::LeftMouseButton);
    }
    else {
        LeftWidgetInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
    }
    PassTriggerToHeldObject(LeftMotionController, AxisVal);
}

void AAzr_Pawn::OnTriggerRight(const FInputActionValue& Value) {
    float AxisVal = Value.Get<float>();
    if (AxisVal > 0.5f) {
        if (RightWidgetInteraction->IsOverHitTestVisibleWidget())
            RightWidgetInteraction->PressPointerKey(EKeys::LeftMouseButton);
    }
    else {
        RightWidgetInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
    }
    PassTriggerToHeldObject(RightMotionController, AxisVal);
}

void AAzr_Pawn::PassTriggerToHeldObject(UMotionControllerComponent* HandController, float Value) {
    if (!HandController) return;

    UAzr_HandScanner* RelevantScanner = (HandController == LeftMotionController) ? LeftHandScanner : RightHandScanner;
    if (!RelevantScanner) return;

    if (UAzr_Latch* HeldLatch = RelevantScanner->GetCurrentHeldLatch()) {
        HeldLatch->HandleTriggerInput(Value);
        return;
    }

    TArray<AActor*> AttachedActors;
    this->GetAttachedActors(AttachedActors);

    for (AActor* AttachedActor : AttachedActors) {
        if (!AttachedActor || !AttachedActor->GetRootComponent()) continue;

        USceneComponent* CurrentParent = AttachedActor->GetRootComponent()->GetAttachParent();

        if (CurrentParent == RelevantScanner || CurrentParent == HandController) {
            if (UAzr_Grab* GrabComp = AttachedActor->FindComponentByClass<UAzr_Grab>()) {
                GrabComp->HandleTriggerInput(Value);
                return;
            }
        }
    }
}

void AAzr_Pawn::GrabLeft(const FInputActionValue& Value) {
    TargetGripLeft = Value.Get<float>();
    if (LeftHandScanner) LeftHandScanner->ProcessGrabInput(true);
}
void AAzr_Pawn::ReleaseLeft(const FInputActionValue& Value) {
    TargetGripLeft = 0.0f;
    if (LeftHandScanner) LeftHandScanner->ProcessGrabInput(false);
}
void AAzr_Pawn::GrabRight(const FInputActionValue& Value) {
    TargetGripRight = Value.Get<float>();
    if (RightHandScanner) RightHandScanner->ProcessGrabInput(true);
}
void AAzr_Pawn::ReleaseRight(const FInputActionValue& Value) {
    TargetGripRight = 0.0f;
    if (RightHandScanner) RightHandScanner->ProcessGrabInput(false);
}

void AAzr_Pawn::SetLeftHandMesh(USkeletalMesh* NewMesh) {
    if (LeftHandMesh && NewMesh) {
        LeftHandMesh->SetSkeletalMesh(NewMesh);
        LeftHandAnim = Cast<UAzr_HandAnimInstance>(LeftHandMesh->GetAnimInstance());
    }
}

void AAzr_Pawn::SetRightHandMesh(USkeletalMesh* NewMesh) {
    if (RightHandMesh && NewMesh) {
        RightHandMesh->SetSkeletalMesh(NewMesh);
        RightHandAnim = Cast<UAzr_HandAnimInstance>(RightHandMesh->GetAnimInstance());
    }
}

void AAzr_Pawn::ProcessGazeTrace() {
    if (!Camera) return;

    FVector StartLoc = Camera->GetComponentLocation();
    FVector EndLoc = StartLoc + (Camera->GetForwardVector() * GazeTraceDistance);

    FCollisionShape Capsule = FCollisionShape::MakeCapsule(GazeTraceRadius, GazeTraceDistance / 2.0f);
    FVector CenterLoc = StartLoc + (Camera->GetForwardVector() * (GazeTraceDistance / 2.0f));
    FQuat CapsuleRot = Camera->GetComponentQuat() * FQuat(FRotator(90.0f, 0.0f, 0.0f));

    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::AllObjects);

    bool bHit = GetWorld()->OverlapMultiByObjectType(OverlapResults, CenterLoc, CapsuleRot, ObjectParams, Capsule, Params);

    bool bFoundGazeTarget = false;
    FVector HitLocation = EndLoc;

    if (bHit) {
        for (const FOverlapResult& Overlap : OverlapResults) {
            if (UAzr_Gaze* GazeComp = Cast<UAzr_Gaze>(Overlap.GetComponent())) {
                GazeComp->SetIsBeingLookedAt();
                bFoundGazeTarget = true;
                HitLocation = Overlap.GetComponent()->GetComponentLocation();
                break;
            }
        }
    }

    if (bShowGazeDebug) {
        FColor DrawColor = bFoundGazeTarget ? FColor::Green : FColor::Red;
        DrawDebugLine(GetWorld(), StartLoc, bFoundGazeTarget ? HitLocation : EndLoc, DrawColor, false, -1.0f, 0, 1.0f);
        if (bFoundGazeTarget) {
            DrawDebugSphere(GetWorld(), HitLocation, GazeTraceRadius, 12, DrawColor, false, -1.0f);
        }
    }
}

void AAzr_Pawn::ExecuteRoomScaleTeleport(FVector TargetFloorLocation) {
    if (!Camera) return;
    FVector CameraOffset = Camera->GetComponentLocation() - GetActorLocation();
    CameraOffset.Z = 0.0f;
    SetActorLocation(TargetFloorLocation - CameraOffset);
}

void AAzr_Pawn::EnableLocomotion() {
    bIsLocomotionEnabled = true;
    if (CachedTeleportComp) CachedTeleportComp->EnableTeleport();
}

void AAzr_Pawn::DisableLocomotion() {
    bIsLocomotionEnabled = false;
    if (CachedTeleportComp) CachedTeleportComp->DisableTeleport();
}

void AAzr_Pawn::Look(const FInputActionValue& Value) {
    if (bIsVRMode) return;
    FVector2D LookInput = Value.Get<FVector2D>();
    AddActorLocalRotation(FRotator(0, LookInput.X, 0));
    FRotator CamRot = Camera->GetRelativeRotation();
    CamRot.Pitch = FMath::Clamp(CamRot.Pitch - LookInput.Y, -80.f, 80.f);
    Camera->SetRelativeRotation(CamRot);
}

// --- NEW UNIFIED LOCOMOTION ROUTING ---

void AAzr_Pawn::Move(const FInputActionValue& Value) {
    if (!bIsLocomotionEnabled) return;

    FVector2D AxisInput = Value.Get<FVector2D>();

    if (bIsVRMode) {
        // --- VR MODE: Route to the Ambidextrous Brain ---
        ProcessStickInput(AxisInput, LeftMotionController, true);
    }
    else {
        // --- DESKTOP MODE: Standard WASD Walking ---
        if (Camera) {
            FVector ViewFwd = Camera->GetForwardVector();
            ViewFwd.Z = 0;
            ViewFwd.Normalize();

            FVector ViewRt = Camera->GetRightVector();
            ViewRt.Z = 0;
            ViewRt.Normalize();

            // Moves the pawn Forward/Back (W/S) and Strafes Left/Right (A/D)
            float DeltaTime = GetWorld()->GetDeltaSeconds();
            AddActorWorldOffset(((ViewFwd * AxisInput.Y) + (ViewRt * AxisInput.X)) * SmoothMoveSpeed * DeltaTime, true);
        }
    }
}

void AAzr_Pawn::RightStickInput(const FInputActionValue& Value) {
    ProcessStickInput(Value.Get<FVector2D>(), RightMotionController, false);
}

void AAzr_Pawn::ProcessStickInput(FVector2D AxisInput, UMotionControllerComponent* HandController, bool bIsLeftHand) {
    if (!bIsLocomotionEnabled || !bIsVRMode) return;

    // --- NEW: THE UI SOFT-LOCK & SCROLLING ---
    // If either hand is currently pointing at a Hit-Test Visible UI Widget, block new locomotion inputs!
    if (bLeftWasHoveringWidget || bRightWasHoveringWidget)
    {
        // 1. Safety Check: If they are mid-teleport while hovering UI, let them release the stick to complete it.
        // If we don't do this, they could get stuck with a teleport arc permanently drawn!
        if (AxisInput.Y > -0.5f && AxisInput.Y < 0.5f && ForwardInput == EForwardBehavior::Teleport && CachedTeleportComp)
        {
            CachedTeleportComp->HandleTeleportInput(0.0f, HandController);
        }

        // 2. THE SCROLL FIX: Translate thumbstick Y-Axis into Mouse Scroll Wheel
        if (FMath::Abs(AxisInput.Y) > 0.1f) // Small deadzone to prevent accidental micro-scrolls
        {
            // Multiply by a scroll speed factor (e.g., 2.0f) to make it feel smooth and responsive
            float ScrollAmount = AxisInput.Y * 2.0f;

            if (bIsLeftHand && bLeftWasHoveringWidget)
            {
                LeftWidgetInteraction->ScrollWheel(ScrollAmount);
            }
            else if (!bIsLeftHand && bRightWasHoveringWidget)
            {
                RightWidgetInteraction->ScrollWheel(ScrollAmount);
            }
        }

        // 3. Consume the input. Stop processing movement math.
        return;
    }

    // --- 1. THE MASTER GATE ---
    // If the joystick being pushed does not match the Dev Setting, kill it instantly!
    bool bIsAllowed = false;
    if (DominantLocomotionHand == EAzr_LocomotionHand::BothHands) bIsAllowed = true;
    else if (DominantLocomotionHand == EAzr_LocomotionHand::LeftHand && bIsLeftHand) bIsAllowed = true;
    else if (DominantLocomotionHand == EAzr_LocomotionHand::RightHand && !bIsLeftHand) bIsAllowed = true;

    if (!bIsAllowed) return; // <-- The unselected hand dies right here.

    float DeltaTime = GetWorld()->GetDeltaSeconds();

    // --- 2. X-AXIS: TURN ---
    float TurnYaw = 0.0f;
    if (FMath::Abs(AxisInput.X) > 0.5f) {
        if (TurnInput == ETurnBehavior::SmoothTurn) {
            TurnYaw = AxisInput.X * SmoothTurnSpeed * DeltaTime;
        }
        else if (TurnInput == ETurnBehavior::SnapTurn && bReadyToSnapTurn) {
            TurnYaw = SnapTurnAngle * (AxisInput.X > 0 ? 1.0f : -1.0f);
            bReadyToSnapTurn = false;
        }
    }

    if (FMath::Abs(TurnYaw) > 0.001f) {
        FVector CameraOffset = Camera->GetComponentLocation() - GetActorLocation();
        CameraOffset.Z = 0.0f;
        FVector RotatedOffset = CameraOffset.RotateAngleAxis(TurnYaw, FVector::UpVector);
        AddActorLocalRotation(FRotator(0.0f, TurnYaw, 0.0f));
        AddActorWorldOffset(CameraOffset - RotatedOffset);
    }

    // --- 3. Y-AXIS: FORWARD & BACKWARD ---
    FVector ViewDir = Camera->GetForwardVector();
    ViewDir.Z = 0; ViewDir.Normalize();

    if (AxisInput.Y > 0.5f) {
        if (ForwardInput == EForwardBehavior::Teleport && CachedTeleportComp) {
            CachedTeleportComp->HandleTeleportInput(AxisInput.Y, HandController);
        }
        else if (ForwardInput == EForwardBehavior::SmoothMove) {
            AddActorWorldOffset(ViewDir * AxisInput.Y * SmoothMoveSpeed * DeltaTime, true);
        }
    }
    else if (AxisInput.Y < -0.5f) {
        // --- EMERGENCY BUG FIX: NAVMESH VALIDATION ---
        UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());

        if (BackwardInput == EBackBehavior::BlinkStep && bReadyToSnapTurn && CachedTeleportComp) {
            FVector TargetLoc = GetActorLocation() - (ViewDir * BlinkStepDistance);
            FNavLocation ProjectedLoc;

            // Only teleport if the destination is safely inside the NavMesh
            if (NavSys && NavSys->ProjectPointToNavigation(TargetLoc, ProjectedLoc, FVector(100.0f, 100.0f, 250.0f))) {
                CachedTeleportComp->TeleportToLocation(TargetLoc);
            }
            bReadyToSnapTurn = false;
        }
        else if (BackwardInput == EBackBehavior::SmoothMove) {
            FVector DeltaMove = ViewDir * AxisInput.Y * SmoothMoveSpeed * DeltaTime;
            FVector TargetLoc = GetActorLocation() + DeltaMove;
            FNavLocation ProjectedLoc;

            // Only allow the micro-step backward if they are still on the NavMesh
            if (NavSys && NavSys->ProjectPointToNavigation(TargetLoc, ProjectedLoc, FVector(50.0f, 50.0f, 250.0f))) {
                AddActorWorldOffset(DeltaMove, true);
            }
        }
    }

    // Y-AXIS: RELEASE TELEPORT
    if (AxisInput.Y > -0.5f && AxisInput.Y < 0.5f && ForwardInput == EForwardBehavior::Teleport && CachedTeleportComp) {
        CachedTeleportComp->HandleTeleportInput(0.0f, HandController);
    }

    // --- 4. RESET LOCKS ---
    if (FMath::Abs(AxisInput.X) < 0.2f && FMath::Abs(AxisInput.Y) < 0.2f) {
        bReadyToSnapTurn = true;
    }
}

void AAzr_Pawn::TeleportPlayer(FVector TargetLocation, FRotator TargetRotation)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !PC->PlayerCameraManager) return;

    // 1. Start Fade to Black
    float FadeTime = 0.2f;
    PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeTime, FLinearColor::Black, false, true);

    TWeakObjectPtr<AAzr_Pawn> WeakThis(this);
    TWeakObjectPtr<APlayerController> WeakPC(PC);

    // 2. Wait for the screen to go pitch black, then teleport safely
    GetWorld()->GetTimerManager().SetTimer(EventTeleportTimerHandle, [WeakThis, WeakPC, TargetLocation, TargetRotation, FadeTime]()
        {
            if (AAzr_Pawn* StrongThis = WeakThis.Get())
            {
                // --- ROOM-SCALE ROTATION CORRECTION ---
                // If the player turned their physical head 90 degrees, we must subtract that 90 degrees 
                // from the Target Rotation so their eyes actually line up with the target!
                if (StrongThis->Camera)
                {
                    float CameraYawOffset = StrongThis->Camera->GetRelativeRotation().Yaw;
                    FRotator NewPawnRot = StrongThis->GetActorRotation();
                    NewPawnRot.Yaw = TargetRotation.Yaw - CameraYawOffset;
                    StrongThis->SetActorRotation(NewPawnRot);
                }
                else
                {
                    StrongThis->SetActorRotation(TargetRotation);
                }

                // --- ROOM-SCALE LOCATION CORRECTION ---
                StrongThis->ExecuteRoomScaleTeleport(TargetLocation);
            }

            // 3. Fade back in
            if (APlayerController* StrongPC = WeakPC.Get())
            {
                if (StrongPC->PlayerCameraManager)
                {
                    StrongPC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FadeTime, FLinearColor::Black, false, false);
                }
            }
        }, FadeTime, false);
}