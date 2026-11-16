
#include "HealthComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = FMath::Max(1.0f, MaxHealth);

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(
			this,
			&UHealthComponent::HandleTakeAnyDamage
		);
	}
}

void UHealthComponent::HandleTakeAnyDamage(
	AActor* DamagedActor,
	float Damage,
	const UDamageType* DamageType,
	AController* InstigatedBy,
	AActor* DamageCauser
)
{
	ApplyHealthDamage(Damage);
}

void UHealthComponent::ApplyHealthDamage(float DamageAmount)
{
	if (DamageAmount <= 0.0f || IsDead())
	{
		return;
	}

	CurrentHealth = FMath::Clamp(
		CurrentHealth - DamageAmount,
		0.0f,
		MaxHealth
	);

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (IsDead())
	{
		OnDeath.Broadcast();
	}
}

bool UHealthComponent::IsDead() const
{
	return CurrentHealth <= 0.0f;
}
