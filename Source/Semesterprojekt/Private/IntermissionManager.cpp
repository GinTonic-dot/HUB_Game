// Fill out your copyright notice in the Description page of Project Settings.


#include "IntermissionManager.h"


// Sets default values
AIntermissionManager::AIntermissionManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AIntermissionManager::BeginPlay()
{
	Super::BeginPlay();

	GameInstanceRef = Cast<UHubGameInstance>(GetGameInstance());

	if (!GameInstanceRef)
	{
		UE_LOG(LogTemp, Error, TEXT("IntermissionManager: Failed to get HubGameInstance."));
		return;
	}

	OnIntermissionReady();
}

bool AIntermissionManager::GetBuildingData(
	EHubBuildingType BuildingType,
	FHubBuildingData& OutBuildingData) const
{
	for (const FHubBuildingData& Data : BuildingData)
	{
		if (Data.BuildingType == BuildingType)
		{
			OutBuildingData = Data;
			return true;
		}
	}

	return false;
}


bool AIntermissionManager::TryPurchaseBuilding(EHubBuildingType BuildingType)
{
	if (!GameInstanceRef)
	{
		return false;
	}

	FHubBuildingData Data;

	if (!GetBuildingData(BuildingType, Data))
	{
		return false;
	}

	if (!GameInstanceRef->PurchaseBuilding(Data))
	{
		return false;
	}

	const int32 NewLevel = GameInstanceRef->GetBuildingLevel(BuildingType);

	OnBuildingPurchased(BuildingType, NewLevel);

	return true;
}