#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DataAsset_InputConfigs.generated.h"

class UInputAction;
USTRUCT(BlueprintType)
struct FInputConfig
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	FGameplayTag InputTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	UInputAction* InputAction = nullptr;
};

class UInputMappingContext;
UCLASS()
class UE_STICKARENADEMO_API UDataAsset_InputConfigs : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UInputAction* FindInputAction(FGameplayTag Tag) const;
	FORCEINLINE UInputMappingContext* GetInputMappng() { return DefaultInputMapping; };

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	UInputMappingContext* DefaultInputMapping;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TArray<FInputConfig> InputConfigs;
};
