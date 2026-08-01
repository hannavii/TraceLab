#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "StalkerCharacter.generated.h"

UCLASS()
class TRACELAB_API AStalkerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AStalkerCharacter();

protected:
    virtual void BeginPlay() override;
};