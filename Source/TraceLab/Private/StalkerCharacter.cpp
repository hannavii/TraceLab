#include "StalkerCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "StalkerAIController.h"

AStalkerCharacter::AStalkerCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    AIControllerClass =
        AStalkerAIController::StaticClass();

    AutoPossessAI =
        EAutoPossessAI::PlacedInWorldOrSpawned;

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    UCharacterMovementComponent* MovementComponent =
        GetCharacterMovement();

    if (MovementComponent)
    {
        MovementComponent->bOrientRotationToMovement = true;

        MovementComponent->RotationRate =
            FRotator(0.0f, 360.0f, 0.0f);

        MovementComponent->MaxWalkSpeed = 350.0f;
    }
}

void AStalkerCharacter::BeginPlay()
{
    Super::BeginPlay();
}