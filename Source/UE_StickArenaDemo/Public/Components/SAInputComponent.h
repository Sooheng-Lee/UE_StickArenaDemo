#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "GamePlay/DataAsset_InputConfigs.h"
#include "SAInputComponent.generated.h"

UCLASS()
class UE_STICKARENADEMO_API USAInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
public:
	template<class UserObject, typename CallbackFunc>
	void BindWithTag(const FGameplayTag& Tag, UDataAsset_InputConfigs* InputConfig, ETriggerEvent TriggerEvent, UserObject* Object, CallbackFunc Func);
};

template<class UserObject, typename CallbackFunc>
inline void USAInputComponent::BindWithTag(const FGameplayTag& Tag, UDataAsset_InputConfigs* InputConfig, ETriggerEvent TriggerEvent, UserObject* Object, CallbackFunc Func)
{
	UInputAction* InputAction = InputConfig->FindInputAction(Tag);
	if (InputAction)
	{
		BindAction(InputAction, TriggerEvent, Object, Func);
	}
}
