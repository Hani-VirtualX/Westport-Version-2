

#include "Azr_Gaze.h"
#include "Azr_Interactable.h"
#include "Azr_Pointer.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/WidgetComponent.h"
#include "CableComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "Azr_Pawn.h" 
#include "Azr_GazeWidget.h"
#include "Azr_Indicator.h" 
#include "Engine/BlueprintGeneratedClass.h" 
#include "GameFramework/PlayerController.h" 
#include "Azr_ExplainWidget.h" 
#include "Azr_ActionWidget.h"
#include "Azr_LabelWidget.h"
// --------------------

UAzr_Gaze::UAzr_Gaze()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// --- AZUREAL COLLISION STANDARD ---
	SetCollisionEnabled(ECollisionEnabled::NoCollision); // Safe by default
	SetCollisionProfileName(FName("NoCollision"));
	InitBoxExtent(FVector(32.0f, 32.0f, 32.0f));

	// --- DEFAULTS ---
	InteractID = 1;
	GazeDuration = 2.0f;
	DrainRate = 1.5f;
	bResetOnTrigger = true;
	HighlightSpeed = 1.2f;
	HighlightMode = EAzr_HighlightMode::AllComponents;
	SpawnedIndicator = nullptr;

	StencilID = 252;
	LastHighlightValue = 0.0f;
	bWasRising = false;

	// Initialize Hover Memory
	bWasLookedAt = false;

	// --- INTERNAL COMPONENTS (Tether System) ---
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
	TetherCable->NumSegments = 1;
	TetherCable->CableLength = 0.0f;

	// --- ASSET INITIALIZATION ---
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPCAsset(TEXT("/AzurealXR/Interaction/Highlight/MPC_Highlight"));
	if (MPCAsset.Succeeded()) HighlightMPC = MPCAsset.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/AzurealXR/Interaction/Cable_System/CableHead"));
	if (SphereMesh.Succeeded()) TetherSettings.AnchorMesh = SphereMesh.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CableMatAsset(TEXT("/AzurealXR/Interaction/Cable_System/M_Cable"));
	if (CableMatAsset.Succeeded()) TetherSettings.CableMaterial = CableMatAsset.Object;

	// --- AUDIO ASSETS ---
	static ConstructorHelpers::FObjectFinder<USoundBase> HighlightStartAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_Start"));
	if (HighlightStartAsset.Succeeded()) SoundHighlightStart = HighlightStartAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> HighlightEndAsset(TEXT("/AzurealXR/Interaction/Highlight/SC_Highlight_End"));
	if (HighlightEndAsset.Succeeded()) SoundHighlightEnd = HighlightEndAsset.Object;

	// --- UPDATED GAZE SOUND PATHWAYS ---
	static ConstructorHelpers::FObjectFinder<USoundBase> GazeStartAsset(TEXT("/AzurealXR/Interaction/Gaze/OnGazeSound"));
	if (GazeStartAsset.Succeeded()) SoundGazeStart = GazeStartAsset.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> GazeCompleteAsset(TEXT("/AzurealXR/Interaction/Gaze/SFX_GazeCompleted"));
	if (GazeCompleteAsset.Succeeded()) SoundGazeCompleted = GazeCompleteAsset.Object;
}

void UAzr_Gaze::BeginPlay()
{
	Super::BeginPlay();
	EnsureInitialized();

	// --- RUNTIME INDICATOR SPAWNING ---
	if (GazeIndicatorClass && GetWorld() && GetWorld()->IsGameWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();

		SpawnedIndicator = GetWorld()->SpawnActor<AAzr_Indicator>(GazeIndicatorClass, GetComponentTransform(), SpawnParams);

		if (SpawnedIndicator)
		{
			SpawnedIndicator->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			SpawnedIndicator->SetActorHiddenInGame(true); // Hidden until EnableGaze is called
		}
	}
}

void UAzr_Gaze::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsGazeEnabled) DisableGaze();
	Super::EndPlay(EndPlayReason);
}

// --- INTERFACE ---

void UAzr_Gaze::EnableGaze()
{
	if (bIsGazeEnabled) return;

	EnsureInitialized();

	bIsGazeEnabled = true;
	bHasTriggered = false;
	CurrentChargeTime = 0.0f;
	LastHighlightValue = 0.0f;
	bWasRising = true;
	bWasLookedAt = false; // Reset hover memory

	// 1. Wake up the Collision so the Pawn can see it
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionProfileName(FName("Azr_Collision"));

	// 2. Wake up World UI & Indicators
	// 2. Wake up World UI & Indicators
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
				ExplainUI->SetExplainText(GazeDescription);
			}
			// 2. Is it an Action Widget?
			else if (UAzr_ActionWidget* ActionUI = Cast<UAzr_ActionWidget>(UserUI))
			{
				ActionUI->SetActionDescription(GazeDescription);
			}
			// 3. Is it a Label Widget?
			else if (UAzr_LabelWidget* LabelUI = Cast<UAzr_LabelWidget>(UserUI))
			{
				LabelUI->SetLabelText(GazeDescription);
			}
		}
	}

	if (SpawnedIndicator) SpawnedIndicator->SetActorHiddenInGame(false);

	// --- THE AUTO-UI BRIDGE ---
	if (AAzr_Pawn* Pawn = Cast<AAzr_Pawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (Pawn->GazeReticleWidget)
		{
			Pawn->GazeReticleWidget->SetVisibility(true);
			if (UAzr_GazeWidget* GazeUI = Cast<UAzr_GazeWidget>(Pawn->GazeReticleWidget->GetUserWidgetObject()))
			{
				GazeUI->SetProgress(0.0f);
			}
		}
	}

	// 3. Activate Visual Feedback & Shift Pointer
	ToggleHighlight(true);
	ToggleTether(true);
	UpdatePointerZOffset(true);
	UpdatePointer(false);

	SetComponentTickEnabled(true);
}

void UAzr_Gaze::DisableGaze()
{
	if (!bIsGazeEnabled) return;

	bIsGazeEnabled = false;
	CurrentChargeTime = 0.0f;
	OnGazeProgressUpdated.Broadcast(0.0f);

	// --- CLEANUP HAPTICS (RAW HARDWARE COMMAND) ---
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Left);
		PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Right);
	}

	// --- CLEANUP AUTO UI & INDICATOR ---
	if (SpawnedIndicator)
	{
		if (bWasLookedAt) SpawnedIndicator->OnShrink();
		SpawnedIndicator->SetActorHiddenInGame(true);
	}

	if (AAzr_Pawn* Pawn = Cast<AAzr_Pawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (Pawn->GazeReticleWidget)
		{
			if (UAzr_GazeWidget* GazeUI = Cast<UAzr_GazeWidget>(Pawn->GazeReticleWidget->GetUserWidgetObject())) GazeUI->SetProgress(0.0f);
			Pawn->GazeReticleWidget->SetVisibility(false);
		}
	}

	// 1. Kill Collision
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionProfileName(FName("NoCollision"));

	// 2. Kill Visuals & Reset Pointer Shift
	ToggleHighlight(false);
	ToggleTether(false);
	if (CurrentTargetWidget) CurrentTargetWidget->SetVisibility(false);

	UpdatePointerZOffset(false);
	if (UAzr_Pointer* Pointer = FindPlayerPointer()) Pointer->DisablePointer();

	SetComponentTickEnabled(false);
}

void UAzr_Gaze::SetIsBeingLookedAt()
{
	bIsBeingLookedAt = true;
}

// --- CORE LOGIC ---

void UAzr_Gaze::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsGazeEnabled) return;

	// --- 1. HOVER ENTER / EXIT (One-Shot Events) ---
	if (bIsBeingLookedAt && !bWasLookedAt)
	{
		if (SpawnedIndicator) SpawnedIndicator->OnExpand();

		// Play sound EVERY time we hover in
		if (SoundGazeStart) UGameplayStatics::SpawnSoundAttached(SoundGazeStart, this);
	}
	else if (!bIsBeingLookedAt && bWasLookedAt)
	{
		if (SpawnedIndicator) SpawnedIndicator->OnShrink();

		// THE CUT-OFF: Stop Dual Vibration instantly when looking away
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Left);
			PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Right);
		}
	}

	// --- 2. DYNAMIC TETHER ---
	if (CurrentTargetWidget && TetherSettings.bEnableTether && EndAnchor)
	{
		if (APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			FVector StartLoc = CurrentTargetWidget->GetComponentLocation();
			FRotator NewRot = UKismetMathLibrary::FindLookAtRotation(StartLoc, CamManager->GetCameraLocation());
			CurrentTargetWidget->SetWorldRotation(NewRot);
		}

		FVector DynamicEndPos = CalculateSurfaceAnchor(CurrentTargetWidget, TetherSettings.WidgetAnchorPos, TetherSettings);
		EndAnchor->SetWorldLocation(DynamicEndPos);
	}

	// --- 3. PROGRESS MATH & CONTINUOUS MAX HAPTICS ---
	if (!bHasTriggered)
	{
		float PreviousTime = CurrentChargeTime;

		if (bIsBeingLookedAt)
		{
			if (CurrentChargeTime == 0.0f)
			{
				UpdatePointer(true);
			}

			CurrentChargeTime += DeltaTime;

			// THE SUSTAIN: Keep sending 1.0 (Max Power) every frame we are looking
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				PC->SetHapticsByValue(1.0f, 1.0f, EControllerHand::Left);
				PC->SetHapticsByValue(1.0f, 1.0f, EControllerHand::Right);
			}
		}
		else
		{
			if (CurrentChargeTime > 0.0f)
			{
				CurrentChargeTime -= (DeltaTime * DrainRate);
				if (CurrentChargeTime <= 0.0f)
				{
					CurrentChargeTime = 0.0f;
					UpdatePointer(false);
				}
			}
		}

		CurrentChargeTime = FMath::Clamp(CurrentChargeTime, 0.0f, GazeDuration);

		if (!FMath::IsNearlyEqual(PreviousTime, CurrentChargeTime, 0.001f))
		{
			float Progress = (GazeDuration > 0.0f) ? (CurrentChargeTime / GazeDuration) : 1.0f;
			OnGazeProgressUpdated.Broadcast(Progress);

			if (AAzr_Pawn* Pawn = Cast<AAzr_Pawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
			{
				if (Pawn->GazeReticleWidget)
				{
					if (UAzr_GazeWidget* GazeUI = Cast<UAzr_GazeWidget>(Pawn->GazeReticleWidget->GetUserWidgetObject()))
					{
						GazeUI->SetProgress(Progress);
					}
				}
			}

			// TRIGGER PAYOFF
			if (Progress >= 1.0f)
			{
				bHasTriggered = true;
				if (SoundGazeCompleted) UGameplayStatics::SpawnSoundAttached(SoundGazeCompleted, this);

				// THE CUT-OFF: Ensure vibration cuts out sharply when gaze completes
				if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
				{
					PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Left);
					PC->SetHapticsByValue(0.0f, 0.0f, EControllerHand::Right);
				}

				OnGazeTriggered.Broadcast();

				if (bResetOnTrigger)
				{
					CurrentChargeTime = 0.0f;
					OnGazeProgressUpdated.Broadcast(0.0f);

					if (AAzr_Pawn* Pawn = Cast<AAzr_Pawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
					{
						if (Pawn->GazeReticleWidget)
						{
							if (UAzr_GazeWidget* GazeUI = Cast<UAzr_GazeWidget>(Pawn->GazeReticleWidget->GetUserWidgetObject())) GazeUI->SetProgress(0.0f);
						}
					}
					bHasTriggered = false;
				}
			}
		}
	}

	// --- 4. HIGHLIGHT PULSE ---
	if (!bIsBeingLookedAt && HighlightMPC)
	{
		if (UWorld* World = GetWorld())
		{
			float CurrentSpeed = AAzr_Interactable::GetGlobalHiveSpeed();
			float Phase = (World->GetTimeSeconds() * CurrentSpeed * UE_TWO_PI) - UE_HALF_PI;
			float CurrentValue = (FMath::Sin(Phase) + 1.0f) / 2.0f;
			UKismetMaterialLibrary::SetScalarParameterValue(World, HighlightMPC, FName("Alpha"), CurrentValue);

			bool bIsRising = (CurrentValue > LastHighlightValue);

			if (CurrentValue < 0.05f && bIsRising && !bWasRising && SoundHighlightStart)
				UGameplayStatics::SpawnSoundAttached(SoundHighlightStart, GetOwner()->GetRootComponent());

			if (bWasRising && !bIsRising && SoundHighlightEnd)
				UGameplayStatics::SpawnSoundAttached(SoundHighlightEnd, GetOwner()->GetRootComponent());

			LastHighlightValue = CurrentValue;
			bWasRising = bIsRising;
		}
	}
	else if (bIsBeingLookedAt && HighlightMPC)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), HighlightMPC, FName("Alpha"), 1.0f);
	}

	// --- 5. THE MAGIC RESET ---
	bWasLookedAt = bIsBeingLookedAt;
	bIsBeingLookedAt = false;
}

// --- LIFECYCLE & HELPERS ---

void UAzr_Gaze::EnsureInitialized()
{
	if (!GetOwner()) return;

	if (StartAnchor->GetAttachParent() == nullptr)
	{
		StartAnchor->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		EndAnchor->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		TetherCable->AttachToComponent(StartAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	if (!TargetMesh) TargetMesh = FindMeshByName(TargetMeshName);

	if (!TargetMesh)
	{
		TArray<UStaticMeshComponent*> MeshComps;
		GetOwner()->GetComponents<UStaticMeshComponent>(MeshComps);
		for (UStaticMeshComponent* Mesh : MeshComps)
		{
			if (Mesh && Mesh != StartAnchor && Mesh != EndAnchor)
			{
				TargetMesh = Mesh;
				break;
			}
		}
	}
}

// --- VISUAL HELPERS ---

void UAzr_Gaze::UpdatePointer(bool bIsLooking)
{
	UAzr_Pointer* Pointer = FindPlayerPointer();
	if (!Pointer) return;

	USceneComponent* Target = TargetMesh ? Cast<USceneComponent>(TargetMesh) : this;
	Pointer->EnablePointer_TargetComponent(Target);
}

void UAzr_Gaze::UpdatePointerZOffset(bool bIsGazeMode)
{
	if (UAzr_Pointer* Pointer = FindPlayerPointer())
	{
		// Safely cast the Pointer to a Scene Component so we can alter its transform in space
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Pointer))
		{
			FVector CurrentLoc = SceneComp->GetRelativeLocation();
			// Retain the X/Y axes, but securely override the Z
			CurrentLoc.Z = bIsGazeMode ? 12.0f : 0.0f;
			SceneComp->SetRelativeLocation(CurrentLoc);
		}
	}
}

void UAzr_Gaze::ToggleTether(bool bState)
{
	if (!bState || !TetherSettings.bEnableTether)
	{
		StartAnchor->SetVisibility(false); EndAnchor->SetVisibility(false); TetherCable->SetVisibility(false); return;
	}

	USceneComponent* MeshTarget = TargetMesh ? Cast<USceneComponent>(TargetMesh) : this;
	USceneComponent* WidgetTarget = FindWidgetByName(TetherSettings.TargetWidgetName);
	if (!MeshTarget || !WidgetTarget) return;

	if (TetherSettings.AnchorMesh) { StartAnchor->SetStaticMesh(TetherSettings.AnchorMesh); EndAnchor->SetStaticMesh(TetherSettings.AnchorMesh); }
	StartAnchor->SetWorldScale3D(FVector(TetherSettings.AnchorScale));
	EndAnchor->SetWorldScale3D(FVector(TetherSettings.AnchorScale));
	if (TetherSettings.CableMaterial) TetherCable->SetMaterial(0, TetherSettings.CableMaterial);
	TetherCable->CableWidth = TetherSettings.CableWidth;

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

	StartAnchor->SetVisibility(true); EndAnchor->SetVisibility(true); TetherCable->SetVisibility(true);
}

void UAzr_Gaze::ToggleHighlight(bool bState)
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

	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh != StartAnchor && Mesh != EndAnchor)
		{
			Mesh->SetRenderCustomDepth(bState);
			Mesh->SetCustomDepthStencilValue(StencilID);
		}
	}
}

// --- EDITOR GHOST SYSTEM ---

void UAzr_Gaze::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld()) GenerateIndicatorPreview();
	else ClearIndicatorPreview();
#endif
}

void UAzr_Gaze::OnComponentCreated()
{
	Super::OnComponentCreated();
#if WITH_EDITOR
	GenerateIndicatorPreview();
#endif
}

void UAzr_Gaze::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld()) GenerateIndicatorPreview();
#endif
}

#if WITH_EDITOR
void UAzr_Gaze::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAzr_Gaze, GazeIndicatorClass))
	{
		ClearIndicatorPreview();
		GenerateIndicatorPreview();
	}
}
#endif

void UAzr_Gaze::ClearIndicatorPreview()
{
	for (UStaticMeshComponent* Mesh : PreviewIndicatorMeshes)
	{
		if (IsValid(Mesh)) Mesh->DestroyComponent();
	}
	PreviewIndicatorMeshes.Empty();
}

void UAzr_Gaze::GenerateIndicatorPreview()
{
	if (!GetWorld() || GetWorld()->IsGameWorld() || IsGarbageCollecting()) return;

	if (!GazeIndicatorClass)
	{
		ClearIndicatorPreview();
		return;
	}

	if (PreviewIndicatorMeshes.Num() > 0 && IsValid(PreviewIndicatorMeshes[0]))
	{
		for (UStaticMeshComponent* Mesh : PreviewIndicatorMeshes)
		{
			if (Mesh && Mesh->GetAttachParent() != this)
			{
				Mesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			}
		}
		return;
	}

	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GazeIndicatorClass);
	if (!BPClass) return;

	TFunction<void(USCS_Node*, FTransform)> ProcessNodes;
	ProcessNodes = [&](USCS_Node* Node, FTransform ParentTransform)
		{
			if (!Node) return;

			FTransform CurrentTransform = ParentTransform;
			if (USceneComponent* Template = Cast<USceneComponent>(Node->GetActualComponentTemplate(BPClass)))
			{
				CurrentTransform = Template->GetRelativeTransform() * ParentTransform;

				if (UStaticMeshComponent* MeshTemplate = Cast<UStaticMeshComponent>(Template))
				{
					FString MeshName = FString::Printf(TEXT("PreviewGaze_%s_%d"), *MeshTemplate->GetName(), FMath::Rand());
					UStaticMeshComponent* NewPreview = NewObject<UStaticMeshComponent>(this, FName(*MeshName));

					if (NewPreview)
					{
						NewPreview->CreationMethod = EComponentCreationMethod::Instance;
						NewPreview->SetFlags(RF_Transient | RF_TextExportTransient);
						NewPreview->RegisterComponent();

						NewPreview->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
						NewPreview->SetRelativeTransform(CurrentTransform);

						NewPreview->SetStaticMesh(MeshTemplate->GetStaticMesh());

						int32 MatCount = MeshTemplate->GetNumMaterials();
						for (int32 i = 0; i < MatCount; i++) NewPreview->SetMaterial(i, MeshTemplate->GetMaterial(i));

						NewPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						NewPreview->SetCastShadow(false);
						PreviewIndicatorMeshes.Add(NewPreview);
					}
				}
			}

			for (USCS_Node* Child : Node->GetChildNodes()) ProcessNodes(Child, CurrentTransform);
		};

	if (USimpleConstructionScript* SCS = BPClass->SimpleConstructionScript)
	{
		for (USCS_Node* RootNode : SCS->GetRootNodes()) ProcessNodes(RootNode, FTransform::Identity);
	}
}

// --- MATH & UTILS ---

FVector UAzr_Gaze::CalculateSurfaceAnchor(USceneComponent* Target, EAzr_TetherPos Pos, const FAzr_TetherConfig& Config)
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

UPrimitiveComponent* UAzr_Gaze::FindMeshByName(FName Name)
{
	if (Name.IsNone()) return nullptr;
	TArray<UPrimitiveComponent*> Comps; GetOwner()->GetComponents(Comps);
	for (UPrimitiveComponent* Comp : Comps) if (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString())) return Comp;
	return nullptr;
}

USceneComponent* UAzr_Gaze::FindWidgetByName(FName Name)
{
	TArray<USceneComponent*> Comps; GetOwner()->GetComponents(Comps);
	for (USceneComponent* Comp : Comps) if (Comp->IsA(UWidgetComponent::StaticClass()) && (Comp->GetFName() == Name || Comp->GetName().Contains(Name.ToString()))) return Cast<USceneComponent>(Comp);
	return nullptr;
}

UAzr_Pointer* UAzr_Gaze::FindPlayerPointer() const
{
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0)) return PlayerPawn->FindComponentByClass<UAzr_Pointer>();
	return nullptr;
}