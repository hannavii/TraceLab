#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TraceEvidenceSubsystem.h"
#include "TraceEvidenceActor.generated.h"

class USceneComponent;
class UAIPerceptionStimuliSourceComponent;

UCLASS()
class TRACELAB_API ATraceEvidenceActor : public AActor
{
    GENERATED_BODY()

public:
    ATraceEvidenceActor();

    void InitializeEvidence(
        const FEvidenceRecord& InEvidenceRecord
    );

    const FEvidenceRecord& GetEvidenceRecord() const;

    bool HasValidEvidence() const;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    USceneComponent* EvidenceRoot;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    UAIPerceptionStimuliSourceComponent* StimuliSourceComponent;

    UPROPERTY(VisibleAnywhere, Category = "Evidence")
    FEvidenceRecord EvidenceRecord;

    UPROPERTY(EditDefaultsOnly, Category = "Evidence")
    float EvidenceLifetime;

    bool bRegisteredWithPerception;
};