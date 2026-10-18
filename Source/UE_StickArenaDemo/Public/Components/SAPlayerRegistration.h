#pragma once

#include "CoreMinimal.h"
#include "SAPlayerRegistration.generated.h"

// Serializable identifiers, never UObject pointers. PlayerId lasts for this pawn session.
USTRUCT(BlueprintType)
struct UE_STICKARENADEMO_API FSAPlayerRegistration
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString PlayerId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString PlayerName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString CharacterClassPath;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString SkeletalMeshPath;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString AnimInstanceClassPath;
};
