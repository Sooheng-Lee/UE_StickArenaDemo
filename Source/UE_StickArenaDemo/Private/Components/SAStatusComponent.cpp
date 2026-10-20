#include "Components/SAStatusComponent.h"
#include "Characters/SABaseCharacter.h"

USAStatusComponent::USAStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USAStatusComponent::ApplyCrystalBlessing(ESACrystalBlessing Blessing, float RemainingDuration)
{
	const ASABaseCharacter* Character = Cast<ASABaseCharacter>(GetOwner());
	if (HP <= 0.0f || (Character && Character->GetCharacterState() == ECharacterState::Dead)) return;
	const float FullDuration = Blessing == ESACrystalBlessing::Healing ? 5.0f : 20.0f;
	const float Duration = RemainingDuration < 0.0f ? FullDuration : FMath::Min(RemainingDuration, FullDuration);
	if (Duration <= 0.0f) return;
	if (Blessing == ESACrystalBlessing::Attack) DefaultDamage += 5.0f;
	else if (Blessing == ESACrystalBlessing::Defense) Defense += 5.0f;
	CrystalEffects.Add({Blessing, Duration});
	SetComponentTickEnabled(true);
}

void USAStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ASABaseCharacter* Character = Cast<ASABaseCharacter>(GetOwner());
	const bool bDead = HP <= 0.0f || (Character && Character->GetCharacterState() == ECharacterState::Dead);
	for (int32 Index = CrystalEffects.Num() - 1; Index >= 0; --Index)
	{
		FCrystalEffect& Effect = CrystalEffects[Index];
		const float Step = FMath::Min(FMath::Max(DeltaTime, 0.0f), Effect.Remaining);
		if (Effect.Type == ESACrystalBlessing::Healing && !bDead)
			HP = FMath::Clamp(HP + 4.0f * Step, 0.0f, FMath::Max(MaxHP, 0.0f));
		Effect.Remaining -= Step;
		if (bDead || Effect.Remaining <= 0.0f)
		{
			// Remove only this bonus, preserving other buffs and changes to base stats.
			if (Effect.Type == ESACrystalBlessing::Attack) DefaultDamage -= 5.0f;
			else if (Effect.Type == ESACrystalBlessing::Defense) Defense -= 5.0f;
			CrystalEffects.RemoveAtSwap(Index);
		}
	}
	if (CrystalEffects.IsEmpty()) SetComponentTickEnabled(false);
}
