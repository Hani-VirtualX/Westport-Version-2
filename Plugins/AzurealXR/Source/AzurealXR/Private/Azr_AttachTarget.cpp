

#include "Azr_AttachTarget.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Azr_Grab.h"
#include "Azr_ExplainWidget.h"
#include "Azr_ActionWidget.h"
#include "Azr_LabelWidget.h"

UAzr_AttachTarget::UAzr_AttachTarget()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	AttachSequenceID = 1;
	bIsFilled = false;

	SnapRadius = 20.0f;
	MagnetSpeed = 10.0f;
	bForceShowTether = false;
	bRequestedVisibility = false;
	bIsSnapping = false;
	SnappingActor = nullptr;
	CachedCameraManager = nullptr;

	TetherSettings.TargetWidgetName = FName("AttachWidget");
	TetherSettings.WidgetGap_Vertical = 0.0f;
	TetherSettings.WidgetGap_Horizontal = 0.0f;
	TetherSettings.AnchorScale = 0.03f;
	TetherSettings.CableWidth = 35.0f;

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(this);
	TriggerSphere->SetSphereRadius(SnapRadius);
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TriggerSphere->SetCollisionProfileName(FName("Azr_Collision"));
	TriggerSphere->SetGenerateOverlapEvents(true);

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
	TetherCable->EndLocation = FVector::ZeroVector;
	TetherCable->CableWidth = TetherSettings.CableWidth;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/AzurealXR/Interaction/Cable_System/CableHead"));
	if (SphereMesh.Succeeded()) TetherSettings.AnchorMesh = SphereMesh.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CableMatAsset(TEXT("/AzurealXR/Interaction/Cable_System/M_Cable"));
	if (CableMatAsset.Succeeded()) TetherSettings.CableMaterial = CableMatAsset.Object;
}

void UAzr_AttachTarget::BeginPlay()
{
	Super::BeginPlay();

	CachedCameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);

	if (GetOwner())
	{
		StartAnchor->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		EndAnchor->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		TetherCable->AttachToComponent(StartAnchor, FAttachmentTransformRules::KeepRelativeTransform);

		if (TriggerSphere)
		{
			TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &UAzr_AttachTarget::OnTriggerOverlap);
			TriggerSphere->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
		}
	}

	StartAnchor->SetVisibility(false);
	EndAnchor->SetVisibility(false);
	TetherCable->SetVisibility(false);

	GenerateGhostFromClass();
	SetGhostVisibility(bRequestedVisibility);
	SetTetherAndWidgetVisibility(bForceShowTether);
}

void UAzr_AttachTarget::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* CurrentActor = SnappingActor.Get();

	if (bIsSnapping && CurrentActor)
	{
		SnapAlpha += DeltaTime * MagnetSpeed;
		float Alpha = FMath::Clamp(SnapAlpha, 0.0f, 1.0f);

		FVector NewLoc = FMath::Lerp(SnapStartLoc, FVector::ZeroVector, Alpha);
		FQuat NewRot = FMath::Lerp(SnapStartRot, FQuat::Identity, Alpha);

		if (CurrentActor->GetRootComponent())
		{
			CurrentActor->SetActorRelativeLocation(NewLoc);
			CurrentActor->SetActorRelativeRotation(NewRot);
		}

		if (Alpha >= 1.0f)
		{
			bIsSnapping = false;
			CurrentActor->SetActorRelativeLocation(FVector::ZeroVector);
			CurrentActor->SetActorRelativeRotation(FQuat::Identity);
			SetSlotFilled(true);

			if (!CurrentWidget || !CurrentWidget->IsVisible())
			{
				SetComponentTickEnabled(false);
			}
		}
	}
	else if (bIsSnapping && !CurrentActor)
	{
		bIsSnapping = false;
		SetComponentTickEnabled(false);
	}

	if (CurrentWidget && CurrentWidget->IsVisible())
	{
		FVector TargetLoc = FVector::ZeroVector;

		if (CachedCameraManager)
		{
			TargetLoc = CachedCameraManager->GetCameraLocation();
		}
		else
		{
			if (APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
			{
				TargetLoc = CamManager->GetCameraLocation();
				CachedCameraManager = CamManager;
			}
		}

		if (!TargetLoc.IsZero())
		{
			FVector StartLoc = CurrentWidget->GetComponentLocation();
			FRotator NewRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, TargetLoc);
			CurrentWidget->SetWorldRotation(NewRot);
		}

		
		// --- FIX: Dynamically update the tether anchor every frame! ---
		// Bypasses the 1-frame Slate bug and allows the tether to adapt to dynamically resized UI
		if (EndAnchor && TetherSettings.bEnableTether && bForceShowTether)
		{
			FVector DynamicEndPos = CalculateSurfaceAnchor(CurrentWidget, TetherSettings.WidgetAnchorPos);
			EndAnchor->SetWorldLocation(DynamicEndPos);

			// --- RIGID OVERRIDE ---
			float ActualDistance = FVector::Dist(StartAnchor->GetComponentLocation(), DynamicEndPos);
			float Slack = TetherSettings.CableHang;

			if (Slack <= 0.1f)
			{
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
}

void UAzr_AttachTarget::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsFilled || bIsSnapping) return;

	if (!CompatibleClass || !OtherActor->GetClass()->IsChildOf(CompatibleClass)) return;

	UAzr_Grab* GrabComp = OtherActor->FindComponentByClass<UAzr_Grab>();
	if (!GrabComp) return;

	GrabComp->ForceRelease();
	GrabComp->DisableGrabAttach();

	if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(OtherActor->GetRootComponent()))
	{
		RootPrim->SetSimulatePhysics(false);
	}

	OtherActor->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform);
	GrabComp->NotifyAttached(AttachSequenceID, this);

	SnappingActor = OtherActor;
	if (AActor* CurrentActor = SnappingActor.Get())
	{
		if (CurrentActor->GetRootComponent())
		{
			SnapStartLoc = CurrentActor->GetRootComponent()->GetRelativeLocation();
			SnapStartRot = CurrentActor->GetRootComponent()->GetRelativeRotation().Quaternion();
		}
	}

	SnapAlpha = 0.0f;
	bIsSnapping = true;
	SetComponentTickEnabled(true);

	SetGhostVisibility(false);
	SetTetherAndWidgetVisibility(false);
}

void UAzr_AttachTarget::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		GenerateGhostFromClass();
		SetGhostVisibility(true);
	}
#endif
}

#if WITH_EDITOR
void UAzr_AttachTarget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (IsGarbageCollecting()) return;

	if (TriggerSphere) TriggerSphere->SetSphereRadius(SnapRadius);

	GenerateGhostFromClass();
	if (GetWorld() && !GetWorld()->IsGameWorld()) SetGhostVisibility(true);
}
#endif

void UAzr_AttachTarget::SetTetherAndWidgetVisibility(bool bShow)
{
	bForceShowTether = bShow;
	if (USceneComponent* WidgetTarget = FindWidgetByName(TetherSettings.TargetWidgetName))
	{
		WidgetTarget->SetVisibility(bShow);
		CurrentWidget = WidgetTarget;

		// --- NEW: INJECT DYNAMIC TEXT WHEN WAKING UP ---
		if (bShow)
		{
			if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(WidgetTarget))
			{
				UUserWidget* UserUI = WidgetComp->GetUserWidgetObject();

				if (UAzr_ExplainWidget* ExplainUI = Cast<UAzr_ExplainWidget>(UserUI))
				{
					ExplainUI->SetExplainText(AttachDescription);
				}
				else if (UAzr_ActionWidget* ActionUI = Cast<UAzr_ActionWidget>(UserUI))
				{
					ActionUI->SetActionDescription(AttachDescription);
				}
				else if (UAzr_LabelWidget* LabelUI = Cast<UAzr_LabelWidget>(UserUI))
				{
					LabelUI->SetLabelText(AttachDescription);
				}
			}
		}
	}

	UpdateTetherVisuals();

	if (bShow)
	{
		SetComponentTickEnabled(true);
	}
	else if (!bIsSnapping)
	{
		SetComponentTickEnabled(false);
	}
}

// --- FIXED: Tether Drift Logic ---
void UAzr_AttachTarget::UpdateTetherVisuals()
{
	bool bShouldShow = TetherSettings.bEnableTether && !bIsFilled && bForceShowTether;

	if (!bShouldShow)
	{
		StartAnchor->SetVisibility(false);
		EndAnchor->SetVisibility(false);
		TetherCable->SetVisibility(false);
		return;
	}

	USceneComponent* WidgetTarget = FindWidgetByName(TetherSettings.TargetWidgetName);
	if (!WidgetTarget) return;

	if (TetherSettings.AnchorMesh)
	{
		StartAnchor->SetStaticMesh(TetherSettings.AnchorMesh);
		EndAnchor->SetStaticMesh(TetherSettings.AnchorMesh);
	}
	StartAnchor->SetWorldScale3D(FVector(TetherSettings.AnchorScale));
	EndAnchor->SetWorldScale3D(FVector(TetherSettings.AnchorScale));

	if (TetherSettings.CableMaterial) TetherCable->SetMaterial(0, TetherSettings.CableMaterial);

	TetherCable->CableWidth = TetherSettings.CableWidth;
	TetherCable->CableLength = 0.0f;

	// --- DRIFT FIX: Lazy Attachment Check ---
	// If anchors are already attached to the correct components, DO NOT recalculate.
	bool bStartCorrect = (StartAnchor->GetAttachParent() == this);
	bool bEndCorrect = (EndAnchor->GetAttachParent() == WidgetTarget);

	if (!bStartCorrect || !bEndCorrect)
	{
		// 1. Calculate ideal World Positions
		FVector StartPos = CalculateSurfaceAnchor(this, TetherSettings.MeshAnchorPos);
		FVector EndPos = CalculateSurfaceAnchor(WidgetTarget, TetherSettings.WidgetAnchorPos);

		// 2. Re-Parent if needed (Mesh anchor attaches to 'this' component)
		if (!bStartCorrect) StartAnchor->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform);
		if (!bEndCorrect) EndAnchor->AttachToComponent(WidgetTarget, FAttachmentTransformRules::KeepWorldTransform);

		// 3. Set Location
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
		bHasTetherSettled = true;
		TetherCable->SetVisibility(false);

		if (UWorld* World = GetWorld()) {
			FTimerHandle SettleTimer;
			World->GetTimerManager().SetTimer(SettleTimer, FTimerDelegate::CreateWeakLambda(this, [this]() {
				// Only show if the slot hasn't been filled during this 0.2s window
				if (!bIsFilled && bForceShowTether && TetherCable) {
					TetherCable->SetVisibility(true);
				}
				}), 0.20f, false);
		}
	}
	else
	{
		TetherCable->SetVisibility(true);
	}
}


// --- MASTERPIECE: Fully Upgraded Local Bounds Math ---
FVector UAzr_AttachTarget::CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos)
{
	if (!Target) return FVector::ZeroVector;

	if (UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(Target))
	{
		// --- FIX: We now ask the engine for the ACTUAL drawn size ---
		FVector2D DrawSize = WidgetComp->GetCurrentDrawSize();
		FVector Scale = WidgetComp->GetComponentScale();
		FVector2D Pivot = WidgetComp->GetPivot();

		float WorldHalfWidth = (DrawSize.X * 0.5f) * Scale.Y;
		float WorldHalfHeight = (DrawSize.Y * 0.5f) * Scale.Z;

		FVector Center = WidgetComp->GetComponentLocation();
		FVector RightVec = WidgetComp->GetRightVector();
		FVector UpVec = WidgetComp->GetUpVector();
		FVector ForwardVec = WidgetComp->GetForwardVector();

		// Adjust for Pivot shift (Unreal Widgets pivot at 0.5, 0.5 usually)
		float PivotShiftX = (0.5f - Pivot.X) * (DrawSize.X * Scale.Y);
		float PivotShiftY = (0.5f - Pivot.Y) * (DrawSize.Y * Scale.Z);
		FVector VisualCenter = Center + (RightVec * PivotShiftX) + (UpVec * PivotShiftY);

		switch (Pos)
		{
		case EAzr_TetherPos::Top: return VisualCenter + (UpVec * (WorldHalfHeight + TetherSettings.WidgetGap_Vertical));
		case EAzr_TetherPos::Bottom: return VisualCenter - (UpVec * (WorldHalfHeight + TetherSettings.WidgetGap_Vertical));
		case EAzr_TetherPos::Right: return VisualCenter + (RightVec * (WorldHalfWidth + TetherSettings.WidgetGap_Horizontal));
		case EAzr_TetherPos::Left: return VisualCenter - (RightVec * (WorldHalfWidth + TetherSettings.WidgetGap_Horizontal));
		case EAzr_TetherPos::Front: return VisualCenter + (ForwardVec * 1.0f);
		case EAzr_TetherPos::Back: return VisualCenter - (ForwardVec * 1.0f);
		default: return VisualCenter;
		}
	}

	// --- MESH LOGIC (Compiler-Safe Local Bounds Math) ---
	FBoxSphereBounds LocalBounds = Target->CalcLocalBounds();
	FVector LocalCenter = LocalBounds.Origin;
	FVector LocalExtents = LocalBounds.BoxExtent;

	// Transform local center out to world space
	FVector WorldCenter = Target->GetComponentTransform().TransformPosition(LocalCenter);
	FVector ForwardVec = Target->GetForwardVector();
	FVector RightVec = Target->GetRightVector();
	FVector UpVec = Target->GetUpVector();
	FVector Scale = Target->GetComponentScale();

	// Multiply our local boundaries by the world scale
	float WorldHalfDepth = LocalExtents.X * FMath::Abs(Scale.X);
	float WorldHalfWidth = LocalExtents.Y * FMath::Abs(Scale.Y);
	float WorldHalfHeight = LocalExtents.Z * FMath::Abs(Scale.Z);

	FVector SurfacePush = FVector::ZeroVector;

	// Calculate which direction to push the anchor and apply the Surface Offset
	switch (Pos)
	{
	case EAzr_TetherPos::Top:
		SurfacePush = UpVec * (WorldHalfHeight + TetherSettings.MeshSurfaceOffset);
		break;
	case EAzr_TetherPos::Bottom:
		SurfacePush = -UpVec * (WorldHalfHeight + TetherSettings.MeshSurfaceOffset);
		break;
	case EAzr_TetherPos::Right:
		SurfacePush = RightVec * (WorldHalfWidth + TetherSettings.MeshSurfaceOffset);
		break;
	case EAzr_TetherPos::Left:
		SurfacePush = -RightVec * (WorldHalfWidth + TetherSettings.MeshSurfaceOffset);
		break;
	case EAzr_TetherPos::Front:
		SurfacePush = ForwardVec * (WorldHalfDepth + TetherSettings.MeshSurfaceOffset);
		break;
	case EAzr_TetherPos::Back:
		SurfacePush = -ForwardVec * (WorldHalfDepth + TetherSettings.MeshSurfaceOffset);
		break;
	case EAzr_TetherPos::Center:
	default:
		break;
	}

	// Finally, add the vertical and horizontal slide offsets 
	return WorldCenter + SurfacePush + (UpVec * TetherSettings.MeshOffset_Vertical) + (RightVec * TetherSettings.MeshOffset_Horizontal);
}

USceneComponent* UAzr_AttachTarget::FindWidgetByName(FName Name)
{
	if (Name.IsNone()) return nullptr;
	if (!GetOwner()) return nullptr;

	TArray<USceneComponent*> Comps;
	GetOwner()->GetComponents(Comps);
	for (USceneComponent* Comp : Comps)
	{
		if (Comp->IsA(UWidgetComponent::StaticClass()))
		{
			if (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString())) return Comp;
		}
	}
	return nullptr;
}

// --- RECURSIVE GHOST GENERATION ---
void UAzr_AttachTarget::GenerateGhostFromClass()
{
	if (!GetWorld() || IsGarbageCollecting()) return;

	for (int32 i = GhostMeshes.Num() - 1; i >= 0; i--)
	{
		if (GhostMeshes[i]) GhostMeshes[i]->DestroyComponent();
	}
	GhostMeshes.Empty();

	// 1. Helper to Spawn Ghost
	auto SpawnGhost = [&](UStaticMesh* Mesh, FTransform RelativeTransform)
		{
			if (!Mesh) return;
			FString GhostName = FString::Printf(TEXT("Ghost_%s_%d"), *Mesh->GetName(), FMath::Rand());
			UStaticMeshComponent* NewGhost = NewObject<UStaticMeshComponent>(this, FName(*GhostName));

			if (NewGhost)
			{
				NewGhost->CreationMethod = EComponentCreationMethod::Instance;
				NewGhost->SetFlags(RF_Transient);
				NewGhost->RegisterComponent();
				NewGhost->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				NewGhost->SetRelativeTransform(RelativeTransform);
				NewGhost->SetStaticMesh(Mesh);

				if (GhostMaterial)
				{
					int32 NumMaterials = NewGhost->GetNumMaterials();
					for (int32 i = 0; i < NumMaterials; i++) NewGhost->SetMaterial(i, GhostMaterial);
				}

				NewGhost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				NewGhost->SetCastShadow(false);
				NewGhost->SetComponentTickEnabled(false);
				GhostMeshes.Add(NewGhost);
			}
		};

	// 2. Custom Override Check
	if (CustomGhostMesh)
	{
		SpawnGhost(CustomGhostMesh, FTransform::Identity);
		return;
	}

	if (!CompatibleClass) return;

	// 3. RECURSIVE SCS PROCESSOR
	TFunction<void(USCS_Node*, FTransform)> ProcessSCSNode;

	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(CompatibleClass);

	ProcessSCSNode = [&](USCS_Node* Node, FTransform ParentTransform)
		{
			if (!Node) return;

			FTransform CurrentTransform = ParentTransform;

			if (USceneComponent* Template = Cast<USceneComponent>(Node->GetActualComponentTemplate(BPClass)))
			{
				CurrentTransform = Template->GetRelativeTransform() * ParentTransform;

				UStaticMeshComponent* MeshTemplate = Cast<UStaticMeshComponent>(Template);
				if (MeshTemplate && MeshTemplate->IsVisible() && !MeshTemplate->bHiddenInGame && !MeshTemplate->ComponentTags.Contains(FName("SkipGhost")))
				{
					SpawnGhost(MeshTemplate->GetStaticMesh(), CurrentTransform);
				}
			}

			for (USCS_Node* ChildNode : Node->GetChildNodes())
			{
				ProcessSCSNode(ChildNode, CurrentTransform);
			}
		};

	// --- PATH A: BLUEPRINT SCS ---
	if (BPClass)
	{
		if (USimpleConstructionScript* SCS = BPClass->SimpleConstructionScript)
		{
			for (USCS_Node* RootNode : SCS->GetRootNodes())
			{
				ProcessSCSNode(RootNode, FTransform::Identity);
			}
		}
	}

	// --- PATH B: NATIVE C++ COMPONENTS ---
	AActor* CDO = Cast<AActor>(CompatibleClass->GetDefaultObject());
	if (CDO)
	{
		TArray<UActorComponent*> NativeComps;
		CDO->GetComponents(UStaticMeshComponent::StaticClass(), NativeComps);

		for (UActorComponent* Comp : NativeComps)
		{
			UStaticMeshComponent* Template = Cast<UStaticMeshComponent>(Comp);

			if (Template && Template->CreationMethod == EComponentCreationMethod::Native &&
				Template->IsVisible() && !Template->bHiddenInGame && !Template->ComponentTags.Contains(FName("SkipGhost")))
			{
				FTransform AccumulatedTransform = Template->GetRelativeTransform();
				USceneComponent* Parent = Template->GetAttachParent();

				while (Parent && Parent != CDO->GetRootComponent())
				{
					AccumulatedTransform = AccumulatedTransform * Parent->GetRelativeTransform();
					Parent = Parent->GetAttachParent();
				}

				SpawnGhost(Template->GetStaticMesh(), AccumulatedTransform);
			}
		}
	}
}

void UAzr_AttachTarget::SetSlotFilled(bool bFilled)
{
	bIsFilled = bFilled;
	SetGhostVisibility(!bIsFilled);
	SetTetherAndWidgetVisibility(false);

	if (bFilled) {
		bHasTetherSettled = false;
	}
}

void UAzr_AttachTarget::SetGhostVisibility(bool bShouldBeVisible)
{
	bRequestedVisibility = bShouldBeVisible;
	bool bFinalState = bShouldBeVisible && !bIsFilled;

	if (bFinalState && GhostMeshes.Num() == 0) GenerateGhostFromClass();

	for (UStaticMeshComponent* Ghost : GhostMeshes)
	{
		if (Ghost) Ghost->SetVisibility(bFinalState);
	}

	if (TriggerSphere)
	{
		ECollisionEnabled::Type NewType = bFinalState ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
		TriggerSphere->SetCollisionEnabled(NewType);
	}

	UpdateTetherVisuals();
}

void UAzr_AttachTarget::NotifyObjectDetached()
{
	bIsSnapping = false;
	SnappingActor = nullptr;
	bIsFilled = false;
	SetTetherAndWidgetVisibility(false);
}