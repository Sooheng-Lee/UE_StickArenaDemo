// Fill out your copyright notice in the Description page of Project Settings.


#include "GamePlay/DataAsset_InputConfigs.h"

UInputAction* UDataAsset_InputConfigs::FindInputAction(FGameplayTag Tag) const
{
	for (FInputConfig InputConfig : InputConfigs)
	{
		if (Tag == InputConfig.InputTag) 
		{
			if (InputConfig.InputAction)
			{
				return InputConfig.InputAction;
			}
		}
	}
	return nullptr;
}
