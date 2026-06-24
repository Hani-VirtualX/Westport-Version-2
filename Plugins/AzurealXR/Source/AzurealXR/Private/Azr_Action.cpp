

#include "Azr_Action.h"
#include "Azr_Interactable.h"
#include "Azr_Pointer.h"
#include "Azr_Pawn.h"
#include "CableComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

UAzr_Action::UAzr_Action()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	InteractID = 1;
	ActionMode = EAzr_ActionMode::Teleport;
	CustomDuration = 3.0f;

	bIsActive = false;
	bIsProcessing = false;
	CurrentProgressTime = 0.0f;
	MaxDuration = 0.0f;

	StencilID = 252;
	LastHighlightValue = 0.0f;
	bWasRising = false;

	ActiveWidgetComp = nullptr;
	ActiveActionUI = nullptr;

	// --- INTERNAL COMPONENTS (Tether System) ---
	StartAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Action_StartAnchor"));
	StartAnchor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StartAnchor->SetCastShadow(false);
	StartAnchor->SetVisibility(false);

	EndAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Action_EndAnchor"));
	EndAnchor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EndAnchor->SetCastShadow(false);
	EndAnchor->SetVisibility(false);

	TetherCable = CreateDefaultSubobject<UCableComponent>(TEXT("Action_TetherCable"));
	TetherCable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TetherCable->SetVisibility(false);
	TetherCable->NumSegments = 20;
	TetherCable->SolverIterations = 4;
	TetherCable->CableLength = 0.0f;

	// --- ASSET INITIALIZATION ---
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPCAsset(TEXT("/AzurealXR/Interaction/Highlight/MPC_Highlight"));
	if (MPCAsset.Succeeded()) HighlightMPC = MPCAsset.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/AzurealXR/Interaction/Cable_System/CableHead"));
	if (SphereMesh.Succeeded()) TetherSettings.AnchorMesh = SphereMesh.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CableMatAsset(TEXT("/AzurealXR/Interaction/Cable_System/M_Cable"));
	if (CableMatAsset.Succeeded()) TetherSettings.CableMaterial = CableMatAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> StartSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_Start"));
	if (StartSoundAsset.Succeeded()) SoundHighlightStart = StartSoundAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> EndSoundAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_End"));
	if (EndSoundAsset.Succeeded()) SoundHighlightEnd = EndSoundAsset.Object;
}

void UAzr_Action::BeginPlay()
{
	Super::BeginPlay();
	EnsureInitialized();

	// Hide widget by default at start
	if (UWidgetComponent* WidgetComp = FindWidgetByName(WidgetName))
	{
		WidgetComp->SetVisibility(false);
		WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WidgetComp->SetCollisionProfileName(FName("NoCollision"));
	}
}

void UAzr_Action::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsActive) DisableAction();
	Super::EndPlay(EndPlayReason);
}

// --- INTERFACE ---

void UAzr_Action::EnableAction()
{
	if (bIsActive) return;

	EnsureInitialized();

	bIsActive = true;
	bIsProcessing = false;
	CurrentProgressTime = 0.0f;
	MaxDuration = 0.0f;

	if (GetWorld())
	{
		LastHighlightValue = 0.0f;
		bWasRising = true;
	}

	TargetMesh = FindMeshByName(TargetMeshName);
	AAzr_Interactable::SetGlobalHiveSpeed(HighlightSpeed);

	// Setup Widget
	ActiveWidgetComp = FindWidgetByName(WidgetName);
	if (ActiveWidgetComp)
	{
		ActiveActionUI = Cast<UAzr_ActionWidget>(ActiveWidgetComp->GetUserWidgetObject());
		if (ActiveActionUI)
		{
			// Initialize the UI state machine based on our selected mode
			ActiveActionUI->InitializeAction(ActionMode);

			// Push the designer's paragraph text into the UI
			ActiveActionUI->SetActionDescription(ActionDescription);

			// Bind the click events to our Execution logic
			ActiveActionUI->OnExecuteClicked.AddDynamic(this, &UAzr_Action::HandleExecuteClicked);
			ActiveActionUI->OnCompletedClicked.AddDynamic(this, &UAzr_Action::HandleCompletedClicked);

			// Wake up UI Collision and Visibility
			ActiveWidgetComp->SetVisibility(true);
			ActiveWidgetComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			ActiveWidgetComp->SetCollisionProfileName(FName("Azr_Collision"));
		}

		
	}

	// Turn on Visuals
	ToggleHighlight(true);
	UpdatePointer(true);
	ToggleTether(true);

	SetComponentTickEnabled(true);
}

void UAzr_Action::DisableAction()
{
	if (!bIsActive) return;

	bIsActive = false;
	bIsProcessing = false;

	// Teardown Widget Bindings
	if (ActiveActionUI)
	{
		ActiveActionUI->OnExecuteClicked.RemoveDynamic(this, &UAzr_Action::HandleExecuteClicked);
		ActiveActionUI->OnCompletedClicked.RemoveDynamic(this, &UAzr_Action::HandleCompletedClicked);
	}

	if (ActiveWidgetComp)
	{
		ActiveWidgetComp->SetVisibility(false);
		ActiveWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ActiveWidgetComp->SetCollisionProfileName(FName("NoCollision"));
	}

	// Clean up Visuals
	ToggleTether(false);
	ToggleHighlight(false);
	UpdatePointer(false);

	SetComponentTickEnabled(false);
}

void UAzr_Action::EnsureInitialized()
{
	if (!GetOwner() || !GetOwner()->GetRootComponent()) return;

	if (StartAnchor->GetAttachParent() == nullptr)
	{
		StartAnchor->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		EndAnchor->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		TetherCable->AttachToComponent(StartAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}

// --- CORE TICK ---

void UAzr_Action::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsActive) return;

	// --- 1. PROGRESS MATH (Animation & Custom Modes) ---
	if (bIsProcessing && ActiveActionUI)
	{
		CurrentProgressTime += DeltaTime;

		if (MaxDuration > 0.0f)
		{
			float Progress = CurrentProgressTime / MaxDuration;
			ActiveActionUI->SetActionProgress(Progress);

			if (CurrentProgressTime >= MaxDuration)
			{
				// We reached the end!
				bIsProcessing = false;
				ActiveActionUI->SetActionCompleted();
				OnActionCompleted.Broadcast();
			}
		}
	}

	// --- 2. DYNAMIC TETHER ---
	if (ActiveWidgetComp && TetherSettings.bEnableTether && EndAnchor)
	{
		// 1. Keep the Billboard logic
		if (APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			FVector StartLoc = ActiveWidgetComp->GetComponentLocation();
			FRotator NewRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, CamManager->GetCameraLocation());
			ActiveWidgetComp->SetWorldRotation(NewRot);
		}

		// 2. Keep the Surface Math
		FVector DynamicEndPos = CalculateSurfaceAnchor(ActiveWidgetComp, TetherSettings.WidgetAnchorPos, TetherSettings);
		EndAnchor->SetWorldLocation(DynamicEndPos);

		// 3. ADD THIS RIGID OVERRIDE (Copied from Explain)
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
	}

	// --- 3. HIGHLIGHT PULSE ---
	if (HighlightMPC)
	{
		if (UWorld* World = GetWorld())
		{
			float CurrentSpeed = AAzr_Interactable::GetGlobalHiveSpeed();
			float Phase = (World->GetTimeSeconds() * CurrentSpeed * UE_TWO_PI) - UE_HALF_PI;
			float CurrentValue = (FMath::Sin(Phase) + 1.0f) / 2.0f;
			UKismetMaterialLibrary::SetScalarParameterValue(World, HighlightMPC, FName("Alpha"), CurrentValue);

			bool bIsNowRising = (CurrentValue > LastHighlightValue);

			if (CurrentValue < 0.05f && bIsNowRising && !bWasRising && SoundHighlightStart)
			{
				UGameplayStatics::SpawnSoundAttached(SoundHighlightStart, GetOwner()->GetRootComponent());
			}

			if (bWasRising && !bIsNowRising && SoundHighlightEnd)
			{
				UGameplayStatics::SpawnSoundAttached(SoundHighlightEnd, GetOwner()->GetRootComponent());
			}

			LastHighlightValue = CurrentValue;
			bWasRising = bIsNowRising;
		}
	}
}

// --- EXECUTION ROUTING ---

void UAzr_Action::HandleExecuteClicked()
{
	// 1. Fire the Start Button Event immediately for all modes
	OnStartButtonPressed.Broadcast();

	// 2. Route the logic based on the mode
	switch (ActionMode)
	{
	case EAzr_ActionMode::Teleport:
		ExecuteTeleport();
		break;
	case EAzr_ActionMode::Animation:
		ExecuteAnimations();
		break;
	case EAzr_ActionMode::Custom:
		ExecuteCustomTimer();
		break;
	}
}

void UAzr_Action::HandleCompletedClicked()
{
	// 1. Fire the Completed Button Event
	OnCompletedButtonPressed.Broadcast();

	// 2. Automatically disable the Action system so the UI disappears cleanly
	DisableAction();
}

void UAzr_Action::ExecuteTeleport()
{
	// 1. As requested: Instantly kill the UI and visuals for clean UX
	DisableAction();

	// Check if the soft pointer is valid
	if (!TeleportTargetPoint.IsValid())
	{
		OnActionCompleted.Broadcast();
		return;
	}

	// 2. Safely delay for 0.5s to let the fade out happen, then teleport
	FTimerHandle TeleportTimerHandle;
	TSoftObjectPtr<AActor> TargetSoftPtr = TeleportTargetPoint;
	TWeakObjectPtr<UAzr_Action> WeakThis(this);

	GetWorld()->GetTimerManager().SetTimer(TeleportTimerHandle, FTimerDelegate::CreateWeakLambda(this, [WeakThis, TargetSoftPtr]()
		{
			if (UAzr_Action* StrongThis = WeakThis.Get())
			{
				// Resolve the soft pointer to get the actual Actor in the level
				if (AActor* Target = TargetSoftPtr.Get())
				{
					if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(StrongThis, 0))
					{
						// Teleport to the exact spot and rotation of the Target Point
						PlayerPawn->SetActorLocationAndRotation(Target->GetActorLocation(), Target->GetActorRotation());
					}
				}
				StrongThis->OnActionCompleted.Broadcast();
			}
		}), 0.5f, false);
}

void UAzr_Action::ExecuteAnimations()
{
	MaxDuration = 0.0f;
	CurrentProgressTime = 0.0f;

	// Fire all assigned animations and find the longest one
	for (const FAzr_ActionAnimPayload& Payload : AnimPayloads)
	{
		// Safely check and resolve the soft pointer
		if (Payload.TargetActor.IsValid() && Payload.AnimToPlay)
		{
			AActor* ResolvedActor = Payload.TargetActor.Get();
			USkeletalMeshComponent* TargetSkelMesh = nullptr;

			// 1. Try to find the specific mesh by name (if the designer typed one in)
			if (!Payload.TargetComponentName.IsNone())
			{
				TArray<USkeletalMeshComponent*> SkelComps;
				ResolvedActor->GetComponents<USkeletalMeshComponent>(SkelComps);
				for (USkeletalMeshComponent* Comp : SkelComps)
				{
					if (Comp && (Comp->GetFName() == Payload.TargetComponentName || Comp->GetName().Contains(Payload.TargetComponentName.ToString())))
					{
						TargetSkelMesh = Comp;
						break;
					}
				}
			}

			// 2. Fallback: If no name was provided (or we didn't find a match), just grab the first one
			if (!TargetSkelMesh)
			{
				TargetSkelMesh = ResolvedActor->FindComponentByClass<USkeletalMeshComponent>();
			}

			// 3. Play the animation
			if (TargetSkelMesh)
			{
				TargetSkelMesh->PlayAnimation(Payload.AnimToPlay, false);

				float AnimLength = Payload.AnimToPlay->GetPlayLength();
				if (AnimLength > MaxDuration)
				{
					MaxDuration = AnimLength;
				}
			}
		}
	}

	if (MaxDuration > 0.0f)
	{
		bIsProcessing = true; // Wakes up the Tick progress bar
	}
	else
	{
		// Failsafe: if array was empty or animations were invalid, complete immediately
		if (ActiveActionUI) ActiveActionUI->SetActionCompleted();
		OnActionCompleted.Broadcast();
	}
}

void UAzr_Action::ExecuteCustomTimer()
{
	MaxDuration = CustomDuration;
	CurrentProgressTime = 0.0f;

	if (MaxDuration > 0.0f)
	{
		bIsProcessing = true; // Wakes up the Tick progress bar
	}
	else
	{
		// Failsafe
		if (ActiveActionUI) ActiveActionUI->SetActionCompleted();
		OnActionCompleted.Broadcast();
	}
}

// --- VISUAL HELPERS ---

void UAzr_Action::UpdatePointer(bool bIsActiveMode)
{
	if (UAzr_Pointer* Pointer = FindPlayerPointer())
	{
		if (bIsActiveMode)
		{
			USceneComponent* Target = TargetMesh ? Cast<USceneComponent>(TargetMesh) : GetOwner()->GetRootComponent();
			Pointer->EnablePointer_TargetComponent(Target);
		}
		else
		{
			Pointer->DisablePointer();
		}
	}
}

void UAzr_Action::ToggleTether(bool bState)
{
	if (!bState || !TetherSettings.bEnableTether)
	{
		StartAnchor->SetVisibility(false);
		EndAnchor->SetVisibility(false);
		TetherCable->SetVisibility(false);
		return;
	}

	USceneComponent* MeshTarget = TargetMesh ? Cast<USceneComponent>(TargetMesh) : GetOwner()->GetRootComponent();
	USceneComponent* WidgetTarget = ActiveWidgetComp;

	if (!MeshTarget || !WidgetTarget) return;

	if (TetherSettings.AnchorMesh)
	{
		StartAnchor->SetStaticMesh(TetherSettings.AnchorMesh);
		EndAnchor->SetStaticMesh(TetherSettings.AnchorMesh);
	}

	StartAnchor->SetWorldScale3D(FVector(TetherSettings.AnchorScale));
	EndAnchor->SetWorldScale3D(FVector(TetherSettings.AnchorScale));

	if (TetherSettings.CableMaterial)
	{
		TetherCable->SetMaterial(0, TetherSettings.CableMaterial);
	}

	TetherCable->CableWidth = TetherSettings.CableWidth;

	// --- PRE-CALCULATE UI ROTATION TO PREVENT FRAME-1 CRASH ---
	// Forces the dormant widget to update its world transform before we calculate anchors
	if (WidgetTarget == ActiveWidgetComp) {
		if (APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0)) {
			FVector StartLoc = WidgetTarget->GetComponentLocation();
			FRotator NewRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, CamManager->GetCameraLocation());
			WidgetTarget->SetWorldRotation(NewRot);
		}
	}

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

	// --- THE FIX: NUKE ALL COLLISION ---
	TetherCable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TetherCable->SetCollisionResponseToAllChannels(ECR_Ignore);
	TetherCable->bEnableCollision = false;

	// --- PERCENTAGE MATH & STIFF HANG ---
	float HangPercentage = TetherSettings.CableHang / 100.0f;
	float InitialDist = FVector::Dist(StartAnchor->GetComponentLocation(), EndAnchor->GetComponentLocation());

	if (HangPercentage <= 0.001f) {
		// 0% Hang = 1 Segment Straight Line
		TetherCable->NumSegments = 1;
		TetherCable->CableLength = InitialDist;
		TetherCable->CableGravityScale = 0.0f;
		TetherCable->bEnableStiffness = false;
	}
	else {
		// > 0% Hang = 20 Segments
		TetherCable->NumSegments = 20;

		// Calculate slack based on the percentage of the distance!
		float SlackAmount = InitialDist * HangPercentage;
		TetherCable->CableLength = InitialDist + SlackAmount;

		// Gentle gravity scaling based on percentage
		TetherCable->CableGravityScale = FMath::Clamp(HangPercentage * 0.5f, 0.01f, 0.5f);

		// --- THE STIFF HANG FIX ---
		TetherCable->bEnableStiffness = true;
		TetherCable->SolverIterations = 16;
	}

	TetherCable->RecreatePhysicsState();

	
	StartAnchor->SetVisibility(true);
	EndAnchor->SetVisibility(true);

	
	TetherCable->SetVisibility(false);

	if (UWorld* World = GetWorld()) {
		FTimerHandle SettleTimer;
		World->GetTimerManager().SetTimer(SettleTimer, FTimerDelegate::CreateWeakLambda(this, [this]() {
			// Only turn it on if the Action UI is still active
			if (bIsActive && TetherCable) {
				TetherCable->SetVisibility(true);
			}
			}), 0.20f, false);
	}
}

void UAzr_Action::ToggleHighlight(bool bState)
{
	if (HighlightMode == EAzr_HighlightMode::None) return;

	TArray<UMeshComponent*> Meshes;
	if (HighlightMode == EAzr_HighlightMode::AllComponents)
	{
		GetOwner()->GetComponents(Meshes);
	}
	else if (TargetMesh)
	{
		Meshes.Add(Cast<UMeshComponent>(TargetMesh));
	}

	if (UMeshComponent* WidgetMesh = Cast<UMeshComponent>(ActiveWidgetComp)) {
		Meshes.AddUnique(WidgetMesh);
	}

	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh != StartAnchor && Mesh != EndAnchor)
		{
			Mesh->SetRenderCustomDepth(bState);
			Mesh->SetCustomDepthStencilValue(StencilID);
		}
	}
}

// --- MATH & UTILS ---

FVector UAzr_Action::CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos, const FAzr_TetherConfig& Config)
{
	if (!Target) return FVector::ZeroVector;

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

UPrimitiveComponent* UAzr_Action::FindMeshByName(FName Name)
{
	if (Name.IsNone() || !GetOwner()) return nullptr;
	TArray<UPrimitiveComponent*> Comps;
	GetOwner()->GetComponents(Comps);
	for (UPrimitiveComponent* Comp : Comps)
	{
		if (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString())) return Comp;
	}
	return nullptr;
}

UWidgetComponent* UAzr_Action::FindWidgetByName(FName Name)
{
	if (Name.IsNone() || !GetOwner()) return nullptr;
	TArray<UWidgetComponent*> Comps;
	GetOwner()->GetComponents<UWidgetComponent>(Comps);
	for (UWidgetComponent* Comp : Comps)
	{
		if (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString())) return Comp;
	}
	return nullptr;
}

UAzr_Pointer* UAzr_Action::FindPlayerPointer() const
{
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return PlayerPawn->FindComponentByClass<UAzr_Pointer>();
	}
	return nullptr;
}