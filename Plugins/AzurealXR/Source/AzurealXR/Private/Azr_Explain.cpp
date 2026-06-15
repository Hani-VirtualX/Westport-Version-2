

#include "Azr_Explain.h"
#include "Azr_Interactable.h"
#include "Azr_Pointer.h"
#include "CableComponent.h"
#include "Components/AudioComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"


UAzr_Explain::UAzr_Explain() {
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    InteractID = 1;
    bIsActive = false;
    bIsPlusMode = false;
    CurrentMiddleIndex = 0;
    StencilID = 252;
    LastHighlightValue = 0.0f;
    bWasRising = false;

    ActiveWidgetComp = nullptr;
    ActiveExplainUI = nullptr;
    ActiveAudioComp = nullptr;

    // --- INTERNAL COMPONENTS (Tether System) ---
    StartAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Explain_StartAnchor"));
    StartAnchor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StartAnchor->SetCastShadow(false);
    StartAnchor->SetVisibility(false);

    EndAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Explain_EndAnchor"));
    EndAnchor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EndAnchor->SetCastShadow(false);
    EndAnchor->SetVisibility(false);

    TetherCable = CreateDefaultSubobject<UCableComponent>(TEXT("Explain_TetherCable"));
    TetherCable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TetherCable->SetVisibility(false);
    TetherCable->NumSegments = 1;
    TetherCable->CableLength = 0.0f;

    // --- ASSET INITIALIZATION ---
    static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPCAsset(TEXT("/AzurealXR/Interaction/Highlight/MPC_Highlight"));
    if (MPCAsset.Succeeded()) HighlightMPC = MPCAsset.Object;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/AzurealXR/Interaction/Cable_System/CableHead"));
    if (SphereMesh.Succeeded()) {
        SingleExplainStep.TetherSettings.AnchorMesh = SphereMesh.Object;
        StartStep.TetherSettings.AnchorMesh = SphereMesh.Object;
        EndStep.TetherSettings.AnchorMesh = SphereMesh.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CableMatAsset(TEXT("/AzurealXR/Interaction/Cable_System/M_Cable"));
    if (CableMatAsset.Succeeded()) {
        SingleExplainStep.TetherSettings.CableMaterial = CableMatAsset.Object;
        StartStep.TetherSettings.CableMaterial = CableMatAsset.Object;
        EndStep.TetherSettings.CableMaterial = CableMatAsset.Object;
    }

    // --- AUDIO ASSETS ---
    static ConstructorHelpers::FObjectFinder<USoundBase> StartSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_Start"));
    if (StartSoundAsset.Succeeded()) SoundHighlightStart = StartSoundAsset.Object;

    static ConstructorHelpers::FObjectFinder<USoundBase> EndSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_End"));
    if (EndSoundAsset.Succeeded()) SoundHighlightEnd = EndSoundAsset.Object;
}

void UAzr_Explain::BeginPlay() {
    Super::BeginPlay();
    EnsureInitialized();
    HideAllWidgets();
}

void UAzr_Explain::EndPlay(const EEndPlayReason::Type EndPlayReason) {
    if (bIsActive) DisableExplain();
    Super::EndPlay(EndPlayReason);
}

// --- INTERFACE ---

void UAzr_Explain::EnableExplain() {
    if (bIsActive) return;

    EnsureInitialized();
    HideAllWidgets();

    bIsActive = true;
    bIsPlusMode = false;

    if (GetWorld()) {
        LastHighlightValue = 0.0f;
        bWasRising = true;
    }

    // Load Step dynamically applies the Mesh, Highlights, and Tethers for this specific step
    LoadStep(SingleExplainStep, EAzr_ExplainStepType::Single);
    SetComponentTickEnabled(true);
}

void UAzr_Explain::EnableExplainPlus() {
    if (bIsActive) return;

    EnsureInitialized();
    HideAllWidgets();

    bIsActive = true;
    bIsPlusMode = true;
    CurrentMiddleIndex = 0;

    if (GetWorld()) {
        LastHighlightValue = 0.0f;
        bWasRising = true;
    }

    // Load Step dynamically applies the Mesh, Highlights, and Tethers for this specific step
    LoadStep(StartStep, EAzr_ExplainStepType::Start);
    SetComponentTickEnabled(true);
}

void UAzr_Explain::DisableExplain() {
    if (!bIsActive) return;

    bIsActive = false;

    if (ActiveAudioComp) ActiveAudioComp->Stop();

    HideAllWidgets();

    // Clean up the visuals from whichever step was currently active
    ToggleTether(false);
    ToggleHighlight(false);
    UpdatePointer(false);

    // FIX: Only broadcast the generic event if this was a Single Explain
    if (!bIsPlusMode) {
        OnExplainCompleted.Broadcast();
    }

    SetComponentTickEnabled(false);
}

// --- CORE LOGIC ---

void UAzr_Explain::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bIsActive) return;

    // --- 1. INDEPENDENT PROGRESS MATH ---
    if (bIsStepRunning && ActiveExplainUI) {
        float Elapsed = GetWorld()->GetTimeSeconds() - StepStartTime;

        if (StepDuration > 0.0f) {
            float Progress = Elapsed / StepDuration;
            ActiveExplainUI->SetAudioProgress(Progress);

            if (Progress >= 1.0f) {
                bIsStepRunning = false;
                ActiveExplainUI->SetPlaybackCompleted();

                // If they used a custom timer that ended BEFORE the audio finished, gracefully stop the audio
                if (ActiveAudioComp && ActiveAudioComp->IsPlaying()) {
                    ActiveAudioComp->Stop();
                }
            }
        }
    }

    // --- 2. DYNAMIC TETHER (Using Step-Specific Config) ---
    if (ActiveWidgetComp && CurrentActiveStep.TetherSettings.bEnableTether && EndAnchor) {
        if (APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0)) {
            FVector StartLoc = ActiveWidgetComp->GetComponentLocation();
            FRotator NewRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, CamManager->GetCameraLocation());
            ActiveWidgetComp->SetWorldRotation(NewRot);
        }

        FVector DynamicEndPos = CalculateSurfaceAnchor(ActiveWidgetComp, CurrentActiveStep.TetherSettings.WidgetAnchorPos, CurrentActiveStep.TetherSettings);
        EndAnchor->SetWorldLocation(DynamicEndPos);
    }

    // --- 3. HIGHLIGHT PULSE ---
    if (HighlightMPC) {
        if (UWorld* World = GetWorld()) {
            float CurrentSpeed = AAzr_Interactable::GetGlobalHiveSpeed();
            float Phase = (World->GetTimeSeconds() * CurrentSpeed * UE_TWO_PI) - UE_HALF_PI;
            float CurrentValue = (FMath::Sin(Phase) + 1.0f) / 2.0f;
            UKismetMaterialLibrary::SetScalarParameterValue(World, HighlightMPC, FName("Alpha"), CurrentValue);

            bool bIsNowRising = (CurrentValue > LastHighlightValue);

            if (CurrentValue < 0.05f && bIsNowRising && !bWasRising && SoundHighlightStart) {
                UGameplayStatics::SpawnSoundAttached(SoundHighlightStart, GetOwner()->GetRootComponent());
            }

            if (bWasRising && !bIsNowRising && SoundHighlightEnd) {
                UGameplayStatics::SpawnSoundAttached(SoundHighlightEnd, GetOwner()->GetRootComponent());
            }

            LastHighlightValue = CurrentValue;
            bWasRising = bIsNowRising;
        }
    }
}

// --- STEP MANAGEMENT ---

void UAzr_Explain::HideAllWidgets() {
    TArray<UWidgetComponent*> Comps;
    if (GetOwner()) GetOwner()->GetComponents<UWidgetComponent>(Comps);

    for (UWidgetComponent* Comp : Comps) {
        // Check against Single, Start, and End steps
        if (Comp->GetFName() == SingleExplainStep.WidgetName ||
            Comp->GetFName() == StartStep.WidgetName ||
            Comp->GetFName() == EndStep.WidgetName) {
            Comp->SetVisibility(false);
            Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Comp->SetCollisionProfileName(FName("NoCollision"));
        }

        // Check against the Middle steps array
        for (const FAzr_ExplainStep& Middle : MiddleSteps) {
            if (Comp->GetFName() == Middle.WidgetName) {
                Comp->SetVisibility(false);
                Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Comp->SetCollisionProfileName(FName("NoCollision"));
            }
        }
    }
}

void UAzr_Explain::LoadStep(const FAzr_ExplainStep& StepData, EAzr_ExplainStepType StepType) {
    // 1. Clean up visuals from the previous step
    ToggleTether(false);
    ToggleHighlight(false);
    UpdatePointer(false);

    // 2. Set the new active data
    CurrentStepType = StepType;
    CurrentActiveStep = StepData;

    // 3. Dynamically update the Target Mesh for this specific step
    TargetMesh = FindMeshByName(CurrentActiveStep.TargetMeshName);
    AAzr_Interactable::SetGlobalHiveSpeed(CurrentActiveStep.HighlightSpeed);

    // 4. Teardown old widget
    if (ActiveWidgetComp) {
        ActiveWidgetComp->SetVisibility(false);
        ActiveWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ActiveWidgetComp->SetCollisionProfileName(FName("NoCollision"));
    }

    if (ActiveExplainUI) {
        ActiveExplainUI->OnPlayAudioClicked.RemoveDynamic(this, &UAzr_Explain::HandlePlayAudioClicked);
        ActiveExplainUI->OnProceedClicked.RemoveDynamic(this, &UAzr_Explain::HandleProceedClicked);
    }

    // 5. Setup new widget
    ActiveWidgetComp = FindWidgetByName(CurrentActiveStep.WidgetName);
    if (ActiveWidgetComp) {
        ActiveExplainUI = Cast<UAzr_ExplainWidget>(ActiveWidgetComp->GetUserWidgetObject());
        if (ActiveExplainUI) {
            ActiveExplainUI->InitializeStep(StepType);

            ActiveExplainUI->SetExplainText(CurrentActiveStep.ExplainText);

            ActiveExplainUI->OnPlayAudioClicked.AddDynamic(this, &UAzr_Explain::HandlePlayAudioClicked);
            ActiveExplainUI->OnProceedClicked.AddDynamic(this, &UAzr_Explain::HandleProceedClicked);

            // --- ACTIVATE VISIBILITY AND COLLISION ---
            ActiveWidgetComp->SetVisibility(true);
            ActiveWidgetComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            ActiveWidgetComp->SetCollisionProfileName(FName("Azr_Collision"));

            // 6. Turn the visuals back on using the NEW step's data
            ToggleHighlight(true);
            UpdatePointer(true);
            ToggleTether(true);
        }
    }
}

void UAzr_Explain::HandlePlayAudioClicked() {

    
    if (CurrentStepType == EAzr_ExplainStepType::Single) OnExplainStarted.Broadcast();
    else if (CurrentStepType == EAzr_ExplainStepType::Start) OnExplainPlusStarted.Broadcast(1);
    else if (CurrentStepType == EAzr_ExplainStepType::Middle) OnExplainPlusStarted.Broadcast(2 + CurrentMiddleIndex);
    else if (CurrentStepType == EAzr_ExplainStepType::End) OnExplainPlusStarted.Broadcast(2 + MiddleSteps.Num());

    
    StepStartTime = GetWorld()->GetTimeSeconds();
    bIsStepRunning = true;
    USoundBase* TrackToPlay = CurrentActiveStep.AudioTrack;

    
    if (CurrentActiveStep.ExplainMode == EAzr_ExplainMode::CustomTimer) {
        StepDuration = CurrentActiveStep.CustomTimerDuration;
    }
    else {
        StepDuration = TrackToPlay ? TrackToPlay->GetDuration() : 0.0f;
    }

   
    if (TrackToPlay && ActiveWidgetComp) {
        if (!ActiveAudioComp) {
            ActiveAudioComp = UGameplayStatics::SpawnSoundAttached(TrackToPlay, ActiveWidgetComp);
        }
        else {
            ActiveAudioComp->SetSound(TrackToPlay);
            ActiveAudioComp->Play();
        }
    }
    else if (StepDuration <= 0.0f) {
        
        bIsStepRunning = false;
        if (ActiveExplainUI) ActiveExplainUI->SetPlaybackCompleted();
    }
}

void UAzr_Explain::HandleProceedClicked() {
    // 1. Single Explain (No Output)
    if (CurrentStepType == EAzr_ExplainStepType::Single) {
        DisableExplain();
    }
    // 2. Start Step (Outputs 1)
    else if (CurrentStepType == EAzr_ExplainStepType::Start) {
        OnExplainPlusCompleted.Broadcast(1);

        if (MiddleSteps.Num() > 0) {
            CurrentMiddleIndex = 0;
            LoadStep(MiddleSteps[CurrentMiddleIndex], EAzr_ExplainStepType::Middle);
        }
        else {
            LoadStep(EndStep, EAzr_ExplainStepType::End); // Safe fallback if no middle steps exist!
        }
    }
    // 3. Middle Steps (Outputs 2, 3, 4...)
    else if (CurrentStepType == EAzr_ExplainStepType::Middle) {
        OnExplainPlusCompleted.Broadcast(2 + CurrentMiddleIndex);

        CurrentMiddleIndex++;
        if (CurrentMiddleIndex < MiddleSteps.Num()) {
            LoadStep(MiddleSteps[CurrentMiddleIndex], EAzr_ExplainStepType::Middle);
        }
        else {
            LoadStep(EndStep, EAzr_ExplainStepType::End);
        }
    }
    // 4. End Step (Outputs Final Count)
    else if (CurrentStepType == EAzr_ExplainStepType::End) {
        OnExplainPlusCompleted.Broadcast(2 + MiddleSteps.Num());
        DisableExplain();
    }
}

// --- VISUAL HELPERS ---

void UAzr_Explain::EnsureInitialized() {
    if (!GetOwner() || !GetOwner()->GetRootComponent()) return;

    if (StartAnchor->GetAttachParent() == nullptr) {
        StartAnchor->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        EndAnchor->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        TetherCable->AttachToComponent(StartAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    }
}

void UAzr_Explain::UpdatePointer(bool bIsActiveMode) {
    if (UAzr_Pointer* Pointer = FindPlayerPointer()) {
        if (bIsActiveMode) {
            USceneComponent* Target = TargetMesh ? Cast<USceneComponent>(TargetMesh) : GetOwner()->GetRootComponent();
            Pointer->EnablePointer_TargetComponent(Target);
        }
        else {
            Pointer->DisablePointer();
        }
    }
}

void UAzr_Explain::ToggleTether(bool bState) {
    if (!bState || !CurrentActiveStep.TetherSettings.bEnableTether) {
        StartAnchor->SetVisibility(false);
        EndAnchor->SetVisibility(false);
        TetherCable->SetVisibility(false);
        return;
    }

    USceneComponent* MeshTarget = TargetMesh ? Cast<USceneComponent>(TargetMesh) : GetOwner()->GetRootComponent();
    USceneComponent* WidgetTarget = ActiveWidgetComp;

    if (!MeshTarget || !WidgetTarget) return;

    if (CurrentActiveStep.TetherSettings.AnchorMesh) {
        StartAnchor->SetStaticMesh(CurrentActiveStep.TetherSettings.AnchorMesh);
        EndAnchor->SetStaticMesh(CurrentActiveStep.TetherSettings.AnchorMesh);
    }

    StartAnchor->SetWorldScale3D(FVector(CurrentActiveStep.TetherSettings.AnchorScale));
    EndAnchor->SetWorldScale3D(FVector(CurrentActiveStep.TetherSettings.AnchorScale));

    if (CurrentActiveStep.TetherSettings.CableMaterial) {
        TetherCable->SetMaterial(0, CurrentActiveStep.TetherSettings.CableMaterial);
    }

    TetherCable->CableWidth = CurrentActiveStep.TetherSettings.CableWidth;

    bool bStartCorrect = (StartAnchor->GetAttachParent() == MeshTarget);
    bool bEndCorrect = (EndAnchor->GetAttachParent() == WidgetTarget);

    if (!bStartCorrect || !bEndCorrect) {
        FVector StartPos = CalculateSurfaceAnchor(MeshTarget, CurrentActiveStep.TetherSettings.MeshAnchorPos, CurrentActiveStep.TetherSettings);
        FVector EndPos = CalculateSurfaceAnchor(WidgetTarget, CurrentActiveStep.TetherSettings.WidgetAnchorPos, CurrentActiveStep.TetherSettings);

        if (!bStartCorrect) StartAnchor->AttachToComponent(MeshTarget, FAttachmentTransformRules::KeepWorldTransform);
        if (!bEndCorrect) EndAnchor->AttachToComponent(WidgetTarget, FAttachmentTransformRules::KeepWorldTransform);

        StartAnchor->SetWorldLocation(StartPos);
        EndAnchor->SetWorldLocation(EndPos);
    }

    TetherCable->SetAttachEndToComponent(EndAnchor);
    TetherCable->SetRelativeLocation(FVector::ZeroVector);
    TetherCable->EndLocation = FVector::ZeroVector;

    StartAnchor->SetVisibility(true);
    EndAnchor->SetVisibility(true);
    TetherCable->SetVisibility(true);
}

void UAzr_Explain::ToggleHighlight(bool bState) {
    if (CurrentActiveStep.HighlightMode == EAzr_HighlightMode::None) return;

    TArray<UMeshComponent*> Meshes;
    if (CurrentActiveStep.HighlightMode == EAzr_HighlightMode::AllComponents) {
        AActor* SearchActor = CurrentActiveStep.ExternalTargetActor.IsValid() ? CurrentActiveStep.ExternalTargetActor.Get() : GetOwner();

        if (SearchActor) {
            SearchActor->GetComponents(Meshes);
        }
    }
    else if (TargetMesh) {
        Meshes.Add(Cast<UMeshComponent>(TargetMesh));
    }

    for (UMeshComponent* Mesh : Meshes) {
        if (Mesh && Mesh != StartAnchor && Mesh != EndAnchor) {
            Mesh->SetRenderCustomDepth(bState);
            Mesh->SetCustomDepthStencilValue(StencilID);
        }
    }
}

// --- MATH & UTILS ---

FVector UAzr_Explain::CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos, const FAzr_TetherConfig& Config) {
    if (!Target) return FVector::ZeroVector;

    if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(Target)) {
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

        switch (Pos) {
        case EAzr_TetherPos::Top: return VisualCenter + (UpVec * (WorldHalfHeight + Config.WidgetGap_Vertical));
        case EAzr_TetherPos::Bottom: return VisualCenter - (UpVec * (WorldHalfHeight + Config.WidgetGap_Vertical));
        case EAzr_TetherPos::Right: return VisualCenter + (RightVec * (WorldHalfWidth + Config.WidgetGap_Horizontal));
        case EAzr_TetherPos::Left: return VisualCenter - (RightVec * (WorldHalfWidth + Config.WidgetGap_Horizontal));
        case EAzr_TetherPos::Front: return VisualCenter + (ForwardVec * 1.0f);
        case EAzr_TetherPos::Back: return VisualCenter - (ForwardVec * 1.0f);
        default: return VisualCenter;
        }
    }

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

    switch (Pos) {
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

UPrimitiveComponent* UAzr_Explain::FindMeshByName(FName Name) {
    if (Name.IsNone()) return nullptr;

    // THE FIX: Use .Get() to resolve the Soft Pointer safely
    AActor* SearchActor = CurrentActiveStep.ExternalTargetActor.IsValid() ? CurrentActiveStep.ExternalTargetActor.Get() : GetOwner();

    if (!SearchActor) return nullptr;

    TArray<UPrimitiveComponent*> Comps;
    SearchActor->GetComponents(Comps);

    for (UPrimitiveComponent* Comp : Comps) {
        if (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString())) return Comp;
    }
    return nullptr;
}

UWidgetComponent* UAzr_Explain::FindWidgetByName(FName Name) {
    if (Name.IsNone() || !GetOwner()) return nullptr;
    TArray<UWidgetComponent*> Comps;
    GetOwner()->GetComponents<UWidgetComponent>(Comps);
    for (UWidgetComponent* Comp : Comps) {
        if (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString())) return Comp;
    }
    return nullptr;
}

UAzr_Pointer* UAzr_Explain::FindPlayerPointer() const {
    if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0)) {
        return PlayerPawn->FindComponentByClass<UAzr_Pointer>();
    }
    return nullptr;
}