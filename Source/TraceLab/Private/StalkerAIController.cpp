#include "StalkerAIController.h"

#include "AITypes.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "TraceEvidenceActor.h"
#include "TraceEvidenceSubsystem.h"

AStalkerAIController::AStalkerAIController()
{
    bSetControlRotationFromPawnOrientation = true;

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;

    AIPerceptionComponent =
        CreateDefaultSubobject<UAIPerceptionComponent>(
            TEXT("AIPerceptionComponent")
        );

    SightConfig =
        CreateDefaultSubobject<UAISenseConfig_Sight>(
            TEXT("SightConfig")
        );

    SightConfig->SightRadius = 1600.0f;
    SightConfig->LoseSightRadius = 1900.0f;
    SightConfig->PeripheralVisionAngleDegrees = 65.0f;
    SightConfig->SetMaxAge(5.0f);

    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

    AIPerceptionComponent->ConfigureSense(*SightConfig);

    AIPerceptionComponent->SetDominantSense(
        SightConfig->GetSenseImplementation()
    );

    SetPerceptionComponent(*AIPerceptionComponent);

    PatrolRadius = 1200.0f;
    PatrolWaitTime = 1.0f;

    InvestigationWaitTime = 2.0f;
    EvidenceMemoryDuration = 90.0f;

    ChaseMoveRefreshInterval = 0.35f;

    AttackRange = 150.0f;
    AttackDamage = 20.0f;
    AttackInterval = 1.0f;

    SearchRadius = 700.0f;
    SearchDuration = 12.0f;
    SearchPointWaitTime = 1.0f;

    bEnablePerceptionDebugLogs = true;

    CurrentState = EStalkerAIState::Patrol;

    TargetPlayer = nullptr;

    bCanSeePlayer = false;
    bPlayerSightSourceRegistered = false;

    LastKnownPlayerLocation = FVector::ZeroVector;
    SearchOrigin = FVector::ZeroVector;

    SearchStartTime = 0.0f;
    LastChaseMoveRequestTime = -BIG_NUMBER;
    LastProcessedEvidenceTime = -1.0f;
    LastSeenEvidenceRecordTime = -BIG_NUMBER;
    LastPerceptionDebugLogTime = -BIG_NUMBER;
}

void AStalkerAIController::BeginPlay()
{
    Super::BeginPlay();

    if (AIPerceptionComponent == nullptr)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[AI Perception] Component is null.")
        );

        return;
    }

    AIPerceptionComponent
        ->OnTargetPerceptionUpdated
        .AddUniqueDynamic(
            this,
            &AStalkerAIController::OnTargetPerceptionUpdated
        );

    AIPerceptionComponent->Activate(true);
    AIPerceptionComponent->SetSenseEnabled(
        UAISense_Sight::StaticClass(),
        true
    );

    EnsurePlayerSightSourceRegistered();

    AIPerceptionComponent->RequestStimuliListenerUpdate();

    GetWorldTimerManager().SetTimerForNextTick(
        this,
        &AStalkerAIController::
        RefreshPerceptionFromCurrentActors
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI Perception] Bound, activated, "
            "and listener update requested."
        )
    );
}

void AStalkerAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI] Possessed Pawn=%s"
        ),
        InPawn ? *InPawn->GetName() : TEXT("None")
    );

    if (AIPerceptionComponent)
    {
        AIPerceptionComponent->Activate(true);

        AIPerceptionComponent->SetSenseEnabled(
            UAISense_Sight::StaticClass(),
            true
        );

        AIPerceptionComponent->RequestStimuliListenerUpdate();
    }

    EnsurePlayerSightSourceRegistered();
    StartPatrol();
}

void AStalkerAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    EnsurePlayerSightSourceRegistered();

    /*
     * Perception 이벤트가 초기 등록 시점을 놓치더라도,
     * AI Perception Component가 현재 감지 중이라고 판단한
     * Actor 목록을 직접 확인합니다.
     */
    RefreshPerceptionFromCurrentActors();

    if (CurrentState == EStalkerAIState::Chase)
    {
        UpdateChase();
        return;
    }

    if (CurrentState != EStalkerAIState::Attack)
    {
        return;
    }

    if (!IsValid(TargetPlayer) || !bCanSeePlayer)
    {
        StartSearching(LastKnownPlayerLocation);
        return;
    }

    LastKnownPlayerLocation =
        TargetPlayer->GetActorLocation();

    const APawn* ControlledPawn = GetPawn();

    if (ControlledPawn == nullptr)
    {
        return;
    }

    const float DistanceSquared = FVector::DistSquared(
        ControlledPawn->GetActorLocation(),
        TargetPlayer->GetActorLocation()
    );

    const float LeaveAttackRange =
        AttackRange * 1.15f;

    if (DistanceSquared >
        FMath::Square(LeaveAttackRange))
    {
        StartChasing(TargetPlayer);
    }
}

void AStalkerAIController::EnsurePlayerSightSourceRegistered()
{
    if (bPlayerSightSourceRegistered)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    APawn* PlayerPawn =
        UGameplayStatics::GetPlayerPawn(this, 0);

    if (PlayerPawn == nullptr)
    {
        return;
    }

    const bool bRegistered =
        UAIPerceptionSystem::
        RegisterPerceptionStimuliSource(
            this,
            UAISense_Sight::StaticClass(),
            PlayerPawn
        );

    bPlayerSightSourceRegistered = bRegistered;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI Perception] Explicit player source "
            "registration Result=%s Actor=%s "
            "PlayerControlled=%s"
        ),
        bRegistered ? TEXT("true") : TEXT("false"),
        *PlayerPawn->GetName(),
        PlayerPawn->IsPlayerControlled()
        ? TEXT("true")
        : TEXT("false")
    );

    if (AIPerceptionComponent)
    {
        AIPerceptionComponent->RequestStimuliListenerUpdate();
    }
}

void
AStalkerAIController::RefreshPerceptionFromCurrentActors()
{
    if (AIPerceptionComponent == nullptr)
    {
        return;
    }

    TArray<AActor*> CurrentlySeenActors;

    AIPerceptionComponent->GetCurrentlyPerceivedActors(
        UAISense_Sight::StaticClass(),
        CurrentlySeenActors
    );

    UWorld* World = GetWorld();

    if (World &&
        bEnablePerceptionDebugLogs &&
        World->GetTimeSeconds() -
        LastPerceptionDebugLogTime >= 1.0f)
    {
        LastPerceptionDebugLogTime =
            World->GetTimeSeconds();

        FString ActorDescription;

        for (AActor* SeenActor : CurrentlySeenActors)
        {
            if (!IsValid(SeenActor))
            {
                continue;
            }

            const APawn* SeenPawn =
                Cast<APawn>(SeenActor);

            ActorDescription += FString::Printf(
                TEXT(
                    "%s(Class=%s, Pawn=%s, "
                    "PlayerControlled=%s) "
                ),
                *SeenActor->GetName(),
                *SeenActor->GetClass()->GetName(),
                SeenPawn ? TEXT("true") : TEXT("false"),
                SeenPawn && SeenPawn->IsPlayerControlled()
                ? TEXT("true")
                : TEXT("false")
            );
        }

        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "[AI Perception Poll] SightCount=%d "
                "Actors=[%s]"
            ),
            CurrentlySeenActors.Num(),
            *ActorDescription
        );
    }

    AActor* VisiblePlayer = nullptr;

    for (AActor* SeenActor : CurrentlySeenActors)
    {
        if (!IsValid(SeenActor))
        {
            continue;
        }

        if (IsPlayerActor(SeenActor))
        {
            VisiblePlayer = SeenActor;
            continue;
        }

        ATraceEvidenceActor* EvidenceActor =
            Cast<ATraceEvidenceActor>(SeenActor);

        if (EvidenceActor == nullptr ||
            !EvidenceActor->HasValidEvidence())
        {
            continue;
        }

        const FEvidenceRecord& Evidence =
            EvidenceActor->GetEvidenceRecord();

        AddKnownEvidence(Evidence);

        if (CurrentState == EStalkerAIState::Patrol ||
            CurrentState == EStalkerAIState::Search)
        {
            TryStartNextEvidenceInvestigation();
        }
    }

    if (VisiblePlayer)
    {
        HandleVisiblePlayerFromPolling(VisiblePlayer);
        return;
    }

    if (bCanSeePlayer &&
        (CurrentState == EStalkerAIState::Chase ||
            CurrentState == EStalkerAIState::Attack))
    {
        HandleLostPlayerFromPolling();
    }
}

void AStalkerAIController::HandleVisiblePlayerFromPolling(
    AActor* PlayerActor
)
{
    if (!IsValid(PlayerActor))
    {
        return;
    }

    const bool bWasAlreadySeeingPlayer =
        bCanSeePlayer &&
        TargetPlayer == PlayerActor;

    TargetPlayer = PlayerActor;
    bCanSeePlayer = true;

    LastKnownPlayerLocation =
        PlayerActor->GetActorLocation();

    if (!bWasAlreadySeeingPlayer)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "[AI Perception Poll] Player acquired: %s"
            ),
            *PlayerActor->GetName()
        );

        RecordLastSeenEvidence(
            LastKnownPlayerLocation
        );
    }

    if (CurrentState != EStalkerAIState::Chase &&
        CurrentState != EStalkerAIState::Attack)
    {
        StartChasing(PlayerActor);
    }
}

void AStalkerAIController::HandleLostPlayerFromPolling()
{
    bCanSeePlayer = false;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI Perception Poll] Player lost. "
            "LastKnown=%s"
        ),
        *LastKnownPlayerLocation.ToString()
    );

    RecordLastSeenEvidence(
        LastKnownPlayerLocation
    );

    StartSearching(
        LastKnownPlayerLocation
    );
}

void AStalkerAIController::OnMoveCompleted(
    FAIRequestID RequestID,
    const FPathFollowingResult& Result
)
{
    Super::OnMoveCompleted(RequestID, Result);

    if (Result.Code != EPathFollowingResult::Success)
    {
        return;
    }

    switch (CurrentState)
    {
    case EStalkerAIState::Patrol:
        GetWorldTimerManager().SetTimer(
            PatrolWaitTimerHandle,
            this,
            &AStalkerAIController::
            MoveToRandomPatrolPoint,
            PatrolWaitTime,
            false
        );
        break;

    case EStalkerAIState::Investigate:
        BeginInvestigationWait();
        break;

    case EStalkerAIState::Chase:
        if (IsValid(TargetPlayer) && bCanSeePlayer)
        {
            const APawn* ControlledPawn = GetPawn();

            if (ControlledPawn)
            {
                const float DistanceSquared =
                    FVector::DistSquared(
                        ControlledPawn->GetActorLocation(),
                        TargetPlayer->GetActorLocation()
                    );

                if (DistanceSquared <=
                    FMath::Square(AttackRange))
                {
                    StartAttacking();
                }
                else
                {
                    UpdateChase();
                }
            }
        }
        break;

    case EStalkerAIState::Search:
        GetWorldTimerManager().SetTimer(
            SearchWaitTimerHandle,
            this,
            &AStalkerAIController::
            MoveToNextSearchPoint,
            SearchPointWaitTime,
            false
        );
        break;

    case EStalkerAIState::Attack:
        break;

    default:
        break;
    }
}

void AStalkerAIController::StartPatrol()
{
    ClearStateTimers();
    StopMovement();

    CurrentState = EStalkerAIState::Patrol;

    TargetPlayer = nullptr;
    bCanSeePlayer = false;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[AI] State -> Patrol")
    );

    MoveToRandomPatrolPoint();
}

void AStalkerAIController::MoveToRandomPatrolPoint()
{
    if (CurrentState != EStalkerAIState::Patrol)
    {
        return;
    }

    APawn* ControlledPawn = GetPawn();
    UWorld* World = GetWorld();

    if (ControlledPawn == nullptr || World == nullptr)
    {
        return;
    }

    UNavigationSystemV1* NavSystem =
        UNavigationSystemV1::GetCurrent(World);

    if (NavSystem == nullptr)
    {
        return;
    }

    FNavLocation RandomLocation;

    const bool bFoundLocation =
        NavSystem->GetRandomReachablePointInRadius(
            ControlledPawn->GetActorLocation(),
            PatrolRadius,
            RandomLocation
        );

    if (!bFoundLocation)
    {
        GetWorldTimerManager().SetTimer(
            PatrolWaitTimerHandle,
            this,
            &AStalkerAIController::
            MoveToRandomPatrolPoint,
            PatrolWaitTime,
            false
        );

        return;
    }

    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(
            RandomLocation.Location,
            50.0f
        );

    if (MoveResult !=
        EPathFollowingRequestResult::RequestSuccessful)
    {
        GetWorldTimerManager().SetTimer(
            PatrolWaitTimerHandle,
            this,
            &AStalkerAIController::
            MoveToRandomPatrolPoint,
            PatrolWaitTime,
            false
        );

        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[AI] Patrol target: %s"),
        *RandomLocation.Location.ToString()
    );
}

void AStalkerAIController::StartInvestigatingEvidence(
    const FEvidenceRecord& Evidence
)
{
    if (Evidence.EvidenceId < 0)
    {
        return;
    }

    if (CurrentState == EStalkerAIState::Chase ||
        CurrentState == EStalkerAIState::Attack)
    {
        return;
    }

    ClearStateTimers();
    StopMovement();

    CurrentState = EStalkerAIState::Investigate;
    CurrentInvestigationEvidence = Evidence;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI] State -> Investigate "
            "Id=%d Location=%s"
        ),
        Evidence.EvidenceId,
        *Evidence.Location.ToString()
    );

    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(
            Evidence.Location,
            80.0f
        );

    if (MoveResult ==
        EPathFollowingRequestResult::AlreadyAtGoal)
    {
        BeginInvestigationWait();
        return;
    }

    if (MoveResult ==
        EPathFollowingRequestResult::Failed)
    {
        InvestigatedEvidenceIds.Add(
            Evidence.EvidenceId
        );

        LastProcessedEvidenceTime =
            FMath::Max(
                LastProcessedEvidenceTime,
                Evidence.CreatedTime
            );

        if (!TryStartNextEvidenceInvestigation())
        {
            StartSearching(Evidence.Location);
        }
    }
}

void AStalkerAIController::BeginInvestigationWait()
{
    if (CurrentState !=
        EStalkerAIState::Investigate)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI] Arrived at evidence Id=%d"
        ),
        CurrentInvestigationEvidence.EvidenceId
    );

    GetWorldTimerManager().SetTimer(
        InvestigationTimerHandle,
        this,
        &AStalkerAIController::FinishInvestigation,
        InvestigationWaitTime,
        false
    );
}

void AStalkerAIController::FinishInvestigation()
{
    if (CurrentState !=
        EStalkerAIState::Investigate)
    {
        return;
    }

    InvestigatedEvidenceIds.Add(
        CurrentInvestigationEvidence.EvidenceId
    );

    LastProcessedEvidenceTime =
        FMath::Max(
            LastProcessedEvidenceTime,
            CurrentInvestigationEvidence.CreatedTime
        );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI] Finished evidence Id=%d"
        ),
        CurrentInvestigationEvidence.EvidenceId
    );

    if (TryStartNextEvidenceInvestigation())
    {
        return;
    }

    StartSearching(
        CurrentInvestigationEvidence.Location
    );
}

bool
AStalkerAIController::TryStartNextEvidenceInvestigation()
{
    if (CurrentState == EStalkerAIState::Chase ||
        CurrentState == EStalkerAIState::Attack)
    {
        return false;
    }

    FEvidenceRecord NextEvidence;

    if (!SelectNextEvidence(NextEvidence))
    {
        return false;
    }

    StartInvestigatingEvidence(NextEvidence);
    return true;
}

bool AStalkerAIController::SelectNextEvidence(
    FEvidenceRecord& OutEvidence
) const
{
    if (KnownEvidenceRecords.IsEmpty())
    {
        return false;
    }

    const UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return false;
    }

    const float CurrentTime = World->GetTimeSeconds();

    bool bFoundEvidence = false;
    float EarliestTime = BIG_NUMBER;
    float BestWeight = -BIG_NUMBER;

    for (const FEvidenceRecord& Evidence :
        KnownEvidenceRecords)
    {
        if (InvestigatedEvidenceIds.Contains(
            Evidence.EvidenceId))
        {
            continue;
        }

        const float EvidenceAge =
            CurrentTime - Evidence.CreatedTime;

        if (EvidenceAge > EvidenceMemoryDuration)
        {
            continue;
        }

        if (Evidence.CreatedTime + KINDA_SMALL_NUMBER <
            LastProcessedEvidenceTime)
        {
            continue;
        }

        const float EvidenceScore =
            Evidence.Weight * Evidence.Confidence;

        const bool bEarlier =
            Evidence.CreatedTime < EarliestTime;

        const bool bSameTimeMoreImportant =
            FMath::IsNearlyEqual(
                Evidence.CreatedTime,
                EarliestTime
            ) &&
            EvidenceScore > BestWeight;

        if (bEarlier || bSameTimeMoreImportant)
        {
            OutEvidence = Evidence;
            EarliestTime = Evidence.CreatedTime;
            BestWeight = EvidenceScore;
            bFoundEvidence = true;
        }
    }

    return bFoundEvidence;
}

void AStalkerAIController::AddKnownEvidence(
    const FEvidenceRecord& Evidence
)
{
    if (Evidence.EvidenceId < 0 ||
        DiscoveredEvidenceIds.Contains(
            Evidence.EvidenceId))
    {
        return;
    }

    DiscoveredEvidenceIds.Add(Evidence.EvidenceId);
    KnownEvidenceRecords.Add(Evidence);

    KnownEvidenceRecords.Sort(
        [](const FEvidenceRecord& Left,
            const FEvidenceRecord& Right)
        {
            return Left.CreatedTime < Right.CreatedTime;
        }
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI] Evidence perceived "
            "Id=%d Type=%d KnownCount=%d"
        ),
        Evidence.EvidenceId,
        static_cast<int32>(Evidence.Type),
        KnownEvidenceRecords.Num()
    );
}

void AStalkerAIController::StartChasing(
    AActor* PlayerActor
)
{
    if (!IsValid(PlayerActor))
    {
        return;
    }

    ClearStateTimers();
    StopMovement();

    TargetPlayer = PlayerActor;
    bCanSeePlayer = true;

    LastKnownPlayerLocation =
        PlayerActor->GetActorLocation();

    CurrentState = EStalkerAIState::Chase;
    LastChaseMoveRequestTime = -BIG_NUMBER;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI] State -> Chase Target=%s"
        ),
        *PlayerActor->GetName()
    );

    UpdateChase();
}

void AStalkerAIController::UpdateChase()
{
    if (CurrentState != EStalkerAIState::Chase)
    {
        return;
    }

    if (!IsValid(TargetPlayer) || !bCanSeePlayer)
    {
        StartSearching(LastKnownPlayerLocation);
        return;
    }

    APawn* ControlledPawn = GetPawn();
    UWorld* World = GetWorld();

    if (ControlledPawn == nullptr || World == nullptr)
    {
        return;
    }

    LastKnownPlayerLocation =
        TargetPlayer->GetActorLocation();

    const float DistanceSquared = FVector::DistSquared(
        ControlledPawn->GetActorLocation(),
        TargetPlayer->GetActorLocation()
    );

    if (DistanceSquared <= FMath::Square(AttackRange))
    {
        StartAttacking();
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime - LastChaseMoveRequestTime <
        ChaseMoveRefreshInterval)
    {
        return;
    }

    LastChaseMoveRequestTime = CurrentTime;

    MoveToActor(
        TargetPlayer,
        AttackRange * 0.8f
    );
}

void AStalkerAIController::StartAttacking()
{
    if (!IsValid(TargetPlayer) || !bCanSeePlayer)
    {
        StartSearching(LastKnownPlayerLocation);
        return;
    }

    ClearStateTimers();
    StopMovement();

    CurrentState = EStalkerAIState::Attack;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[AI] State -> Attack")
    );

    PerformAttack();

    if (CurrentState == EStalkerAIState::Attack)
    {
        GetWorldTimerManager().SetTimer(
            AttackTimerHandle,
            this,
            &AStalkerAIController::PerformAttack,
            AttackInterval,
            true
        );
    }
}

void AStalkerAIController::PerformAttack()
{
    if (!IsValid(TargetPlayer) || !bCanSeePlayer)
    {
        StartSearching(LastKnownPlayerLocation);
        return;
    }

    APawn* ControlledPawn = GetPawn();

    if (ControlledPawn == nullptr)
    {
        return;
    }

    const float DistanceSquared = FVector::DistSquared(
        ControlledPawn->GetActorLocation(),
        TargetPlayer->GetActorLocation()
    );

    if (DistanceSquared > FMath::Square(AttackRange))
    {
        StartChasing(TargetPlayer);
        return;
    }

    UGameplayStatics::ApplyDamage(
        TargetPlayer,
        AttackDamage,
        this,
        ControlledPawn,
        UDamageType::StaticClass()
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI] Attack executed "
            "Damage=%.1f Target=%s"
        ),
        AttackDamage,
        *TargetPlayer->GetName()
    );
}

void AStalkerAIController::StartSearching(
    const FVector& SearchLocation
)
{
    ClearStateTimers();
    StopMovement();

    CurrentState = EStalkerAIState::Search;

    TargetPlayer = nullptr;
    bCanSeePlayer = false;

    SearchOrigin = SearchLocation;

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    SearchStartTime = World->GetTimeSeconds();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI] State -> Search Origin=%s"
        ),
        *SearchOrigin.ToString()
    );

    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(
            SearchOrigin,
            80.0f
        );

    if (MoveResult ==
        EPathFollowingRequestResult::AlreadyAtGoal)
    {
        GetWorldTimerManager().SetTimer(
            SearchWaitTimerHandle,
            this,
            &AStalkerAIController::
            MoveToNextSearchPoint,
            SearchPointWaitTime,
            false
        );

        return;
    }

    if (MoveResult ==
        EPathFollowingRequestResult::Failed)
    {
        MoveToNextSearchPoint();
    }
}

void AStalkerAIController::MoveToNextSearchPoint()
{
    if (CurrentState != EStalkerAIState::Search)
    {
        return;
    }

    APawn* ControlledPawn = GetPawn();
    UWorld* World = GetWorld();

    if (ControlledPawn == nullptr || World == nullptr)
    {
        return;
    }

    const float SearchElapsedTime =
        World->GetTimeSeconds() - SearchStartTime;

    if (SearchElapsedTime >= SearchDuration)
    {
        FinishSearch();
        return;
    }

    UNavigationSystemV1* NavSystem =
        UNavigationSystemV1::GetCurrent(World);

    if (NavSystem == nullptr)
    {
        FinishSearch();
        return;
    }

    FNavLocation SearchPoint;

    const bool bFoundPoint =
        NavSystem->GetRandomReachablePointInRadius(
            SearchOrigin,
            SearchRadius,
            SearchPoint
        );

    if (!bFoundPoint)
    {
        FinishSearch();
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI] Search point: %s"
        ),
        *SearchPoint.Location.ToString()
    );

    const EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(
            SearchPoint.Location,
            60.0f
        );

    if (MoveResult !=
        EPathFollowingRequestResult::RequestSuccessful)
    {
        GetWorldTimerManager().SetTimer(
            SearchWaitTimerHandle,
            this,
            &AStalkerAIController::
            MoveToNextSearchPoint,
            SearchPointWaitTime,
            false
        );
    }
}

void AStalkerAIController::FinishSearch()
{
    if (CurrentState != EStalkerAIState::Search)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[AI] Search finished.")
    );

    if (TryStartNextEvidenceInvestigation())
    {
        return;
    }

    StartPatrol();
}

void AStalkerAIController::RecordLastSeenEvidence(
    const FVector& Location
)
{
    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime - LastSeenEvidenceRecordTime < 0.25f)
    {
        return;
    }

    LastSeenEvidenceRecordTime = CurrentTime;

    UTraceEvidenceSubsystem* EvidenceSubsystem =
        World->GetSubsystem<UTraceEvidenceSubsystem>();

    if (EvidenceSubsystem == nullptr)
    {
        return;
    }

    const FEvidenceRecord LastSeenEvidence =
        EvidenceSubsystem->AddEvidence(
            EEvidenceType::LastSeen,
            Location,
            1.0f,
            100.0f
        );

    AddKnownEvidence(LastSeenEvidence);

    LastProcessedEvidenceTime =
        LastSeenEvidence.CreatedTime;
}

void AStalkerAIController::ClearStateTimers()
{
    GetWorldTimerManager().ClearTimer(
        PatrolWaitTimerHandle
    );

    GetWorldTimerManager().ClearTimer(
        InvestigationTimerHandle
    );

    GetWorldTimerManager().ClearTimer(
        SearchWaitTimerHandle
    );

    GetWorldTimerManager().ClearTimer(
        AttackTimerHandle
    );
}

void AStalkerAIController::OnTargetPerceptionUpdated(
    AActor* Actor,
    FAIStimulus Stimulus
)
{
    if (Actor == nullptr)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[AI Perception Event] "
            "Actor=%s Class=%s Sensed=%s"
        ),
        *Actor->GetName(),
        *Actor->GetClass()->GetName(),
        Stimulus.WasSuccessfullySensed()
        ? TEXT("true")
        : TEXT("false")
    );

    if (IsPlayerActor(Actor))
    {
        HandlePlayerPerception(Actor, Stimulus);
        return;
    }

    ATraceEvidenceActor* EvidenceActor =
        Cast<ATraceEvidenceActor>(Actor);

    if (EvidenceActor)
    {
        HandleEvidencePerception(
            EvidenceActor,
            Stimulus
        );
    }
}

void AStalkerAIController::HandlePlayerPerception(
    AActor* Actor,
    const FAIStimulus& Stimulus
)
{
    if (Stimulus.WasSuccessfullySensed())
    {
        HandleVisiblePlayerFromPolling(Actor);
        return;
    }

    if (bCanSeePlayer &&
        (CurrentState == EStalkerAIState::Chase ||
            CurrentState == EStalkerAIState::Attack))
    {
        HandleLostPlayerFromPolling();
    }
}

void AStalkerAIController::HandleEvidencePerception(
    ATraceEvidenceActor* EvidenceActor,
    const FAIStimulus& Stimulus
)
{
    if (!Stimulus.WasSuccessfullySensed())
    {
        return;
    }

    if (EvidenceActor == nullptr ||
        !EvidenceActor->HasValidEvidence())
    {
        return;
    }

    const FEvidenceRecord& Evidence =
        EvidenceActor->GetEvidenceRecord();

    AddKnownEvidence(Evidence);

    if (CurrentState == EStalkerAIState::Patrol ||
        CurrentState == EStalkerAIState::Search)
    {
        TryStartNextEvidenceInvestigation();
    }
}

bool AStalkerAIController::IsPlayerActor(
    AActor* Actor
) const
{
    if (Actor == nullptr)
    {
        return false;
    }

    const APawn* DetectedPawn =
        Cast<APawn>(Actor);

    if (DetectedPawn == nullptr)
    {
        return false;
    }

    return DetectedPawn->IsPlayerControlled();
}