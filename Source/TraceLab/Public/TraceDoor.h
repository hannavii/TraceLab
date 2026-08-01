#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TraceDoor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;
class ATraceEvidenceActor;

UCLASS()
class TRACELAB_API ATraceDoor : public AActor
{
    GENERATED_BODY()

public:
    ATraceDoor();

    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Door")
    void OpenDoor();

    UFUNCTION(BlueprintCallable, Category = "Door")
    void CloseDoor();

    UFUNCTION(BlueprintCallable, Category = "Door")
    void ToggleDoor();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* DoorRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* DoorPivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* DoorMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* InteractionBox;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    bool bAutoOpenOnBeginPlay;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    float OpenAngle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
    float OpenSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Size")
    float DoorThickness;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Size")
    float DoorWidth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Size")
    float DoorHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Evidence")
    float EvidenceCooldown;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Door|Evidence"
    )
    TSubclassOf<ATraceEvidenceActor> EvidenceActorClass;

    UFUNCTION()
    void OnInteractionBoxBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    UFUNCTION()
    void OnInteractionBoxEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex
    );

private:
    bool bIsOpen;

    float LastOpenEvidenceTime;

    FRotator ClosedPivotRotation;
    FRotator OpenPivotRotation;

    void UpdateDoorMeshTransform();

    void CreateDoorEvidence();

    bool IsPlayerActor(AActor* Actor) const;
};