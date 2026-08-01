#include "TraceEvidenceActor.h"

#include "Components/SceneComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

ATraceEvidenceActor::ATraceEvidenceActor()
{
    PrimaryActorTick.bCanEverTick = false;

    EvidenceRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("EvidenceRoot")
    );

    RootComponent = EvidenceRoot;

    StimuliSourceComponent =
        CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(
            TEXT("StimuliSourceComponent")
        );

    EvidenceLifetime = 90.0f;
    bRegisteredWithPerception = false;

    SetActorEnableCollision(false);
}

void ATraceEvidenceActor::BeginPlay()
{
    Super::BeginPlay();

    SetLifeSpan(EvidenceLifetime);
}

void ATraceEvidenceActor::InitializeEvidence(
    const FEvidenceRecord& InEvidenceRecord
)
{
    EvidenceRecord = InEvidenceRecord;
    SetActorLocation(EvidenceRecord.Location);

    if (!bRegisteredWithPerception && StimuliSourceComponent)
    {
        StimuliSourceComponent->RegisterForSense(
            UAISense_Sight::StaticClass()
        );

        StimuliSourceComponent->RegisterWithPerceptionSystem();

        bRegisteredWithPerception = true;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[Evidence Actor] Registered Id=%d Location=%s"
        ),
        EvidenceRecord.EvidenceId,
        *EvidenceRecord.Location.ToString()
    );
}

const FEvidenceRecord&
ATraceEvidenceActor::GetEvidenceRecord() const
{
    return EvidenceRecord;
}

bool ATraceEvidenceActor::HasValidEvidence() const
{
    return EvidenceRecord.EvidenceId >= 0;
}