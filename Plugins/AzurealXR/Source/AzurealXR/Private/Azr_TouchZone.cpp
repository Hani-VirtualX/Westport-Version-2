

#include "Azr_TouchZone.h"
#include "Azr_Indicator.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"

UAzr_TouchZone::UAzr_TouchZone()
{
	// As a collision zone, we don't need to tick
	PrimaryComponentTick.bCanEverTick = false;

	// Set sensible defaults for the box size
	BoxExtent = FVector(10.0f, 10.0f, 10.0f);

	// Start with no collision; the Azr_Touch component will wake this up when enabled
	SetCollisionProfileName(FName("NoCollision"));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Azureal Touch Green
	ShapeColor = FColor(0, 255, 150, 255);
	InteractID = 1;
	SpawnedIndicator = nullptr;
}

void UAzr_TouchZone::BeginPlay()
{
	Super::BeginPlay();

	// --- RUNTIME INDICATOR SPAWNING (RELATIVE) ---
	if (TouchIndicatorClass && GetWorld() && GetWorld()->IsGameWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();

		// Spawn the actual gameplay actor at our current location
		SpawnedIndicator = GetWorld()->SpawnActor<AAzr_Indicator>(TouchIndicatorClass, GetComponentTransform(), SpawnParams);

		if (SpawnedIndicator)
		{
			// Glue it to the box so it follows movement/rotation
			// NotIncludingScale ensures the indicator doesn't stretch if the box is resized
			SpawnedIndicator->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

			// Keep it hidden until the Azr_Touch logic brain calls EnableTouch()
			SpawnedIndicator->SetActorHiddenInGame(true);
		}
	}
}

// --- EDITOR GHOST SYSTEM (COMPONENT OVERRIDES) ---

void UAzr_TouchZone::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		GenerateIndicatorPreview();
	}
	else
	{
		// Clean up the ghosts immediately when the game starts
		ClearIndicatorPreview();
	}
#endif
}

void UAzr_TouchZone::OnComponentCreated()
{
	Super::OnComponentCreated();
#if WITH_EDITOR
	GenerateIndicatorPreview();
#endif
}

void UAzr_TouchZone::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		GenerateIndicatorPreview();
	}
#endif
}

#if WITH_EDITOR
void UAzr_TouchZone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// Refresh the ghost if the designer changes the class in the dropdown
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAzr_TouchZone, TouchIndicatorClass))
	{
		ClearIndicatorPreview();
		GenerateIndicatorPreview();
	}
}
#endif

// --- EDITOR GHOST PROCESSOR (STABILIZED) ---

void UAzr_TouchZone::ClearIndicatorPreview()
{
	for (UStaticMeshComponent* Mesh : PreviewIndicatorMeshes)
	{
		if (IsValid(Mesh))
		{
			Mesh->DestroyComponent();
		}
	}
	PreviewIndicatorMeshes.Empty();
}

void UAzr_TouchZone::GenerateIndicatorPreview()
{
	if (!GetWorld() || GetWorld()->IsGameWorld() || IsGarbageCollecting()) return;

	if (!TouchIndicatorClass)
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

	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(TouchIndicatorClass);
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
					FString MeshName = FString::Printf(TEXT("PreviewTouch_%s_%d"), *MeshTemplate->GetName(), FMath::Rand());
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
						for (int32 i = 0; i < MatCount; i++)
						{
							NewPreview->SetMaterial(i, MeshTemplate->GetMaterial(i));
						}

						NewPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						NewPreview->SetCastShadow(false);
						PreviewIndicatorMeshes.Add(NewPreview);
					}
				}
			}

			for (USCS_Node* Child : Node->GetChildNodes())
			{
				ProcessNodes(Child, CurrentTransform);
			}
		};

	if (USimpleConstructionScript* SCS = BPClass->SimpleConstructionScript)
	{
		for (USCS_Node* RootNode : SCS->GetRootNodes())
		{
			ProcessNodes(RootNode, FTransform::Identity);
		}
	}
}