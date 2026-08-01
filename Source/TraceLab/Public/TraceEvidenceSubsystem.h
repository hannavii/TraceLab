#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TraceEvidenceSubsystem.generated.h"

UENUM(BlueprintType)
enum class EEvidenceType : uint8
{
    DoorOpened UMETA(DisplayName = "Door Opened"),
    LightChanged UMETA(DisplayName = "Light Changed"),
    ItemTaken UMETA(DisplayName = "Item Taken"),
    Noise UMETA(DisplayName = "Noise"),
    LastSeen UMETA(DisplayName = "Last Seen")
};

USTRUCT(BlueprintType)
struct FEvidenceRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Evidence")
    int32 EvidenceId = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Evidence")
    EEvidenceType Type = EEvidenceType::DoorOpened;

    UPROPERTY(BlueprintReadOnly, Category = "Evidence")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Evidence")
    float CreatedTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Evidence")
    float Confidence = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Evidence")
    float Weight = 1.0f;
};

UCLASS()
class TRACELAB_API UTraceEvidenceSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Evidence")
    FEvidenceRecord AddEvidence(
        EEvidenceType Type,
        FVector Location,
        float Confidence,
        float Weight
    );

    bool GetLatestEvidence(FEvidenceRecord& OutEvidence) const;

    bool GetEvidenceById(
        int32 EvidenceId,
        FEvidenceRecord& OutEvidence
    ) const;

    bool GetEvidenceInRadius(
        const FVector& SearchLocation,
        float Radius,
        TArray<FEvidenceRecord>& OutEvidenceRecords
    ) const;

    void RemoveExpiredEvidence(float MaxAge);

private:
    UPROPERTY()
    TArray<FEvidenceRecord> EvidenceRecords;

    int32 NextEvidenceId = 0;
};