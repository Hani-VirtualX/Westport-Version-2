

#include "Azr_LatchZone.h"
#include "Azr_Indicator.h"
#include "Azr_Latch.h" // Added so the Zone knows about the Brain
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"

UAzr_LatchZone::UAzr_LatchZone()
{
	PrimaryComponentTick.bCanEverTick = false;

	BoxExtent = FVector(10.0f, 10.0f, 10.0f);
	SetCollisionProfileName(FName("NoCollision"));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShapeColor = FColor(0, 200, 255, 255);

	// --- NEW DEFAULTS ---
	InteractID = 1;
	CurrentActiveLatch = nullptr;
	SpawnedLatchIndicator = nullptr;
}

void UAzr_LatchZone::BeginPlay()
{
	Super::BeginPlay();

	// --- BIND OUR OWN OVERLAPS ---
	OnComponentBeginOverlap.AddDynamic(this, &UAzr_LatchZone::OnLatchZoneBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &UAzr_LatchZone::OnLatchZoneEndOverlap);

	// --- SPAWN OUR OWN INDICATOR ---
	if (LatchIndicatorClass && GetWorld() && GetWorld()->IsGameWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();

		SpawnedLatchIndicator = GetWorld()->SpawnActor<AAzr_Indicator>(LatchIndicatorClass, GetComponentTransform(), SpawnParams);

		if (SpawnedLatchIndicator)
		{
			SpawnedLatchIndicator->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			SpawnedLatchIndicator->SetActorHiddenInGame(true);
		}
	}
}

// --- SMART HUB OVERLAPS ---

void UAzr_LatchZone::OnLatchZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!SpawnedLatchIndicator || !OtherComp) return;

	// Verify this is actually a VR Hand
	if (OtherComp->ComponentHasTag(FName("Left")) || OtherComp->ComponentHasTag(FName("Right")))
	{
		int32 HandCount = 0;
		TArray<UPrimitiveComponent*> OverlappingComps;

		// We are the zone now, so we just ask ourselves for overlaps
		GetOverlappingComponents(OverlappingComps);

		for (UPrimitiveComponent* Comp : OverlappingComps)
		{
			if (Comp && (Comp->ComponentHasTag(FName("Left")) || Comp->ComponentHasTag(FName("Right"))))
			{
				HandCount++;
			}
		}

		// Only expand if this is the very first hand entering
		if (HandCount == 1)
		{
			SpawnedLatchIndicator->OnExpand();
		}
	}
}

void UAzr_LatchZone::OnLatchZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!SpawnedLatchIndicator || !OtherComp) return;

	// Verify a VR Hand is what just left
	if (OtherComp->ComponentHasTag(FName("Left")) || OtherComp->ComponentHasTag(FName("Right")))
	{
		int32 HandCount = 0;
		TArray<UPrimitiveComponent*> OverlappingComps;

		GetOverlappingComponents(OverlappingComps);

		for (UPrimitiveComponent* Comp : OverlappingComps)
		{
			if (Comp && (Comp->ComponentHasTag(FName("Left")) || Comp->ComponentHasTag(FName("Right"))))
			{
				HandCount++;
			}
		}

		// Only shrink if the box is now completely empty of hands
		if (HandCount == 0)
		{
			SpawnedLatchIndicator->OnShrink();
		}
	}
}

// --- EDITOR GHOST SYSTEM (COMPONENT OVERRIDES) ---

void UAzr_LatchZone::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld()) { GenerateIndicatorPreview(); }
	else { ClearIndicatorPreview(); }
#endif
}

void UAzr_LatchZone::OnComponentCreated()
{
	Super::OnComponentCreated();
#if WITH_EDITOR
	GenerateIndicatorPreview();
#endif
}

void UAzr_LatchZone::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld()) { GenerateIndicatorPreview(); }
#endif
}

#if WITH_EDITOR
void UAzr_LatchZone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAzr_LatchZone, LatchIndicatorClass))
	{
		ClearIndicatorPreview();
		GenerateIndicatorPreview();
	}
}
#endif

// --- EDITOR GHOST PROCESSOR (STABILIZED) ---

void UAzr_LatchZone::ClearIndicatorPreview()
{
	for (UStaticMeshComponent* Mesh : PreviewIndicatorMeshes) { if (IsValid(Mesh)) Mesh->DestroyComponent(); }
	PreviewIndicatorMeshes.Empty();
}

void UAzr_LatchZone::GenerateIndicatorPreview()
{
	if (!GetWorld() || GetWorld()->IsGameWorld() || IsGarbageCollecting()) return;
	if (!LatchIndicatorClass) { ClearIndicatorPreview(); return; }

	if (PreviewIndicatorMeshes.Num() > 0 && IsValid(PreviewIndicatorMeshes[0]))
	{
		for (UStaticMeshComponent* Mesh : PreviewIndicatorMeshes)
		{
			if (Mesh && Mesh->GetAttachParent() != this) Mesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		return;
	}

	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(LatchIndicatorClass);
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
					FString MeshName = FString::Printf(TEXT("PreviewInd_%s_%d"), *MeshTemplate->GetName(), FMath::Rand());
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