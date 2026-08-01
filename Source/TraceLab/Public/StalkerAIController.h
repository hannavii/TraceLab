#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "TimerManager.h"
#include "TraceEvidenceSubsystem.h"
#include "StalkerAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class ATraceEvidenceActor;

UENUM(BlueprintType)
enum class EStalkerAIState : uint8
{
    Patrol UMETA(DisplayName = "Patrol"),
    Investigate UMETA(DisplayName = "Investigate"),
    Chase UMETA(DisplayName = "Chase"),
    Search UMETA(DisplayName = "Search"),
    Attack UMETA(DisplayName = "Attack")
};

UCLASS()
class TRACELAB_API AStalkerAIController : public AAIController
{
    GENERATED_BODY()

public:
    AStalkerAIController();

protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaSeconds) override;

    virtual void OnMoveCompleted(
        FAIRequestID RequestID,
        const FPathFollowingResult& Result
    ) override;

private:
    UFUNCTION()
    void OnTargetPerceptionUpdated(
        AActor* Actor,
        FAIStimulus Stimulus
    );

    void EnsurePlayerSightSourceRegistered();
    void RefreshPerceptionFromCurrentActors();

    void HandlePlayerPerception(
        AActor* Actor,
        const FAIStimulus& Stimulus
    );

    void HandleEvidencePerception(
        ATraceEvidenceActor* EvidenceActor,
        const FAIStimulus& Stimulus
    );

    void HandleVisiblePlayerFromPolling(AActor* PlayerActor);
    void HandleLostPlayerFromPolling();

    void StartPatrol();
    void MoveToRandomPatrolPoint();

    void StartInvestigatingEvidence(
        const FEvidenceRecord& Evidence
    );

    void BeginInvestigationWait();
    void FinishInvestigation();

    bool TryStartNextEvidenceInvestigation();

    bool SelectNextEvidence(
        FEvidenceRecord& OutEvidence
    ) const;

    void AddKnownEvidence(
        const FEvidenceRecord& Evidence
    );

    void StartChasing(AActor* PlayerActor);
    void UpdateChase();

    void StartAttacking();
    void PerformAttack();

    void StartSearching(
        const FVector& SearchLocation
    );

    void MoveToNextSearchPoint();
    void FinishSearch();

    void RecordLastSeenEvidence(
        const FVector& Location
    );

    void ClearStateTimers();

    bool IsPlayerActor(AActor* Actor) const;

private:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "AI|Perception",
        meta = (AllowPrivateAccess = "true")
    )
    UAIPerceptionComponent* AIPerceptionComponent;

    UPROPERTY()
    UAISenseConfig_Sight* SightConfig;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "AI|State",
        meta = (AllowPrivateAccess = "true")
    )
    EStalkerAIState CurrentState;

    UPROPERTY()
    AActor* TargetPlayer;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Patrol")
    float PatrolRadius;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Patrol")
    float PatrolWaitTime;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Evidence")
    float InvestigationWaitTime;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Evidence")
    float EvidenceMemoryDuration;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Chase")
    float ChaseMoveRefreshInterval;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Attack")
    float AttackRange;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Attack")
    float AttackDamage;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Attack")
    float AttackInterval;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Search")
    float SearchRadius;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Search")
    float SearchDuration;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Search")
    float SearchPointWaitTime;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Debug")
    bool bEnablePerceptionDebugLogs;

    bool bCanSeePlayer;
    bool bPlayerSightSourceRegistered;

    FVector LastKnownPlayerLocation;
    FVector SearchOrigin;

    float SearchStartTime;
    float LastChaseMoveRequestTime;
    float LastProcessedEvidenceTime;
    float LastSeenEvidenceRecordTime;
    float LastPerceptionDebugLogTime;

    FTimerHandle PatrolWaitTimerHandle;
    FTimerHandle InvestigationTimerHandle;
    FTimerHandle SearchWaitTimerHandle;
    FTimerHandle AttackTimerHandle;

    UPROPERTY()
    TSet<int32> DiscoveredEvidenceIds;

    UPROPERTY()
    TSet<int32> InvestigatedEvidenceIds;

    UPROPERTY()
    TArray<FEvidenceRecord> KnownEvidenceRecords;

    UPROPERTY()
    FEvidenceRecord CurrentInvestigationEvidence;
};