#include "TraceEvidenceSubsystem.h"

#include "Engine/World.h"

FEvidenceRecord UTraceEvidenceSubsystem::AddEvidence(
    EEvidenceType Type,
    FVector Location,
    float Confidence,
    float Weight
)
{
    // 지나치게 오래된 월드 증거는 정리합니다.
    RemoveExpiredEvidence(300.0f);

    FEvidenceRecord NewEvidence;

    NewEvidence.EvidenceId = NextEvidenceId++;
    NewEvidence.Type = Type;
    NewEvidence.Location = Location;
    NewEvidence.Confidence = Confidence;
    NewEvidence.Weight = Weight;

    if (const UWorld* World = GetWorld())
    {
        NewEvidence.CreatedTime = World->GetTimeSeconds();
    }

    EvidenceRecords.Add(NewEvidence);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[Evidence] Added Id=%d Type=%d Location=%s "
            "Confidence=%.2f Weight=%.2f"
        ),
        NewEvidence.EvidenceId,
        static_cast<int32>(NewEvidence.Type),
        *NewEvidence.Location.ToString(),
        NewEvidence.Confidence,
        NewEvidence.Weight
    );

    return NewEvidence;
}

bool UTraceEvidenceSubsystem::GetLatestEvidence(
    FEvidenceRecord& OutEvidence
) const
{
    if (EvidenceRecords.IsEmpty())
    {
        return false;
    }

    OutEvidence = EvidenceRecords.Last();
    return true;
}

bool UTraceEvidenceSubsystem::GetEvidenceById(
    int32 EvidenceId,
    FEvidenceRecord& OutEvidence
) const
{
    for (const FEvidenceRecord& Evidence : EvidenceRecords)
    {
        if (Evidence.EvidenceId == EvidenceId)
        {
            OutEvidence = Evidence;
            return true;
        }
    }

    return false;
}

bool UTraceEvidenceSubsystem::GetEvidenceInRadius(
    const FVector& SearchLocation,
    float Radius,
    TArray<FEvidenceRecord>& OutEvidenceRecords
) const
{
    OutEvidenceRecords.Reset();

    const float RadiusSquared = FMath::Square(Radius);

    for (const FEvidenceRecord& Evidence : EvidenceRecords)
    {
        const float DistanceSquared = FVector::DistSquared(
            SearchLocation,
            Evidence.Location
        );

        if (DistanceSquared <= RadiusSquared)
        {
            OutEvidenceRecords.Add(Evidence);
        }
    }

    return !OutEvidenceRecords.IsEmpty();
}

void UTraceEvidenceSubsystem::RemoveExpiredEvidence(float MaxAge)
{
    UWorld* World = GetWorld();

    if (World == nullptr || MaxAge <= 0.0f)
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    EvidenceRecords.RemoveAll(
        [CurrentTime, MaxAge](const FEvidenceRecord& Evidence)
        {
            return CurrentTime - Evidence.CreatedTime > MaxAge;
        }
    );
}