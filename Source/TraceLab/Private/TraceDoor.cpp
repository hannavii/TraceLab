#include "TraceDoor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TraceEvidenceActor.h"
#include "TraceEvidenceSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ATraceDoor::ATraceDoor()
{
    PrimaryActorTick.bCanEverTick = true;

    DoorRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("DoorRoot")
    );

    RootComponent = DoorRoot;

    DoorPivot = CreateDefaultSubobject<USceneComponent>(
        TEXT("DoorPivot")
    );

    DoorPivot->SetupAttachment(DoorRoot);

    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("DoorMesh")
    );

    DoorMesh->SetupAttachment(DoorPivot);

    InteractionBox = CreateDefaultSubobject<UBoxComponent>(
        TEXT("InteractionBox")
    );

    InteractionBox->SetupAttachment(DoorRoot);
    InteractionBox->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly
    );

    InteractionBox->SetCollisionResponseToAllChannels(
        ECR_Ignore
    );

    InteractionBox->SetCollisionResponseToChannel(
        ECC_Pawn,
        ECR_Overlap
    );

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube")
    );

    if (CubeMesh.Succeeded())
    {
        DoorMesh->SetStaticMesh(CubeMesh.Object);
    }

    EvidenceActorClass = ATraceEvidenceActor::StaticClass();

    bAutoOpenOnBeginPlay = false;
    OpenAngle = 90.0f;
    OpenSpeed = 3.0f;
    bIsOpen = false;

    LastOpenEvidenceTime = -9999.0f;
    EvidenceCooldown = 3.0f;

    DoorThickness = 15.0f;
    DoorWidth = 100.0f;
    DoorHeight = 220.0f;

    InteractionBox->SetBoxExtent(
        FVector(120.0f, 120.0f, 120.0f)
    );

    InteractionBox->SetRelativeLocation(
        FVector(
            0.0f,
            DoorWidth * 0.5f,
            DoorHeight * 0.5f
        )
    );

    UpdateDoorMeshTransform();
}

void ATraceDoor::BeginPlay()
{
    Super::BeginPlay();

    UpdateDoorMeshTransform();

    ClosedPivotRotation = DoorPivot->GetRelativeRotation();

    OpenPivotRotation =
        ClosedPivotRotation
        + FRotator(0.0f, OpenAngle, 0.0f);

    InteractionBox->OnComponentBeginOverlap.AddDynamic(
        this,
        &ATraceDoor::OnInteractionBoxBeginOverlap
    );

    InteractionBox->OnComponentEndOverlap.AddDynamic(
        this,
        &ATraceDoor::OnInteractionBoxEndOverlap
    );

    if (bAutoOpenOnBeginPlay)
    {
        OpenDoor();
    }
}

void ATraceDoor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const FRotator TargetRotation =
        bIsOpen
        ? OpenPivotRotation
        : ClosedPivotRotation;

    const FRotator NewRotation = FMath::RInterpTo(
        DoorPivot->GetRelativeRotation(),
        TargetRotation,
        DeltaTime,
        OpenSpeed
    );

    DoorPivot->SetRelativeRotation(NewRotation);
}

void ATraceDoor::OpenDoor()
{
    if (bIsOpen)
    {
        return;
    }

    bIsOpen = true;

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime - LastOpenEvidenceTime < EvidenceCooldown)
    {
        return;
    }

    LastOpenEvidenceTime = CurrentTime;

    CreateDoorEvidence();
}

void ATraceDoor::CloseDoor()
{
    if (!bIsOpen)
    {
        return;
    }

    bIsOpen = false;
}

void ATraceDoor::ToggleDoor()
{
    if (bIsOpen)
    {
        CloseDoor();
    }
    else
    {
        OpenDoor();
    }
}

void ATraceDoor::CreateDoorEvidence()
{
    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    UTraceEvidenceSubsystem* EvidenceSubsystem =
        World->GetSubsystem<UTraceEvidenceSubsystem>();

    if (EvidenceSubsystem == nullptr)
    {
        return;
    }

    /*
     * 문 Actor의 원점은 경첩이나 바닥에 있을 수 있습니다.
     * AI가 벽이나 문짝 안쪽의 점을 보려고 하지 않도록
     * 상호작용 박스 중심에서 문 앞쪽으로 약간 이동시킵니다.
     */
    const FVector EvidenceLocation =
        InteractionBox->GetComponentLocation()
        + GetActorForwardVector() * 50.0f;

    const FEvidenceRecord NewEvidence =
        EvidenceSubsystem->AddEvidence(
            EEvidenceType::DoorOpened,
            EvidenceLocation,
            1.0f,
            30.0f
        );

    TSubclassOf<ATraceEvidenceActor> ClassToSpawn =
        EvidenceActorClass;

    if (!ClassToSpawn)
    {
        ClassToSpawn = ATraceEvidenceActor::StaticClass();
    }

    FActorSpawnParameters SpawnParameters;

    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ATraceEvidenceActor* EvidenceActor =
        World->SpawnActor<ATraceEvidenceActor>(
            ClassToSpawn.Get(),
            EvidenceLocation,
            FRotator::ZeroRotator,
            SpawnParameters
        );

    if (EvidenceActor)
    {
        EvidenceActor->InitializeEvidence(NewEvidence);
    }
}

void ATraceDoor::UpdateDoorMeshTransform()
{
    DoorMesh->SetRelativeScale3D(
        FVector(
            DoorThickness / 100.0f,
            DoorWidth / 100.0f,
            DoorHeight / 100.0f
        )
    );

    DoorMesh->SetRelativeLocation(
        FVector(
            0.0f,
            DoorWidth * 0.5f,
            DoorHeight * 0.5f
        )
    );

    DoorMesh->SetRelativeRotation(
        FRotator::ZeroRotator
    );
}

void ATraceDoor::OnInteractionBoxBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    if (IsPlayerActor(OtherActor))
    {
        OpenDoor();
    }
}

void ATraceDoor::OnInteractionBoxEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex
)
{
    if (IsPlayerActor(OtherActor))
    {
        CloseDoor();
    }
}

bool ATraceDoor::IsPlayerActor(AActor* Actor) const
{
    if (Actor == nullptr)
    {
        return false;
    }

    const APawn* Pawn = Cast<APawn>(Actor);

    if (Pawn == nullptr)
    {
        return false;
    }

    return Pawn->IsPlayerControlled();
}