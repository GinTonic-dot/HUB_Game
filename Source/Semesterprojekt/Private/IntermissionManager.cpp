// Fill out your copyright notice in the Description page of Project Settings.


#include "IntermissionManager.h"


AIntermissionManager::AIntermissionManager()
{
	PrimaryActorTick.bCanEverTick = false;
}


void AIntermissionManager::BeginPlay()
{
	Super::BeginPlay();

	GameInstanceRef = Cast<UHubGameInstance>(GetGameInstance());

	if (!GameInstanceRef)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("IntermissionManager: Failed to get HubGameInstance.")
		);

		return;
	}

	OnIntermissionReady();
}


bool AIntermissionManager::GetBuildingData(
	EHubBuildingType BuildingType,
	FHubBuildingData& OutBuildingData
) const
{
	if (!GameInstanceRef)
	{
		return false;
	}

	return GameInstanceRef->GetBuildingData(
		BuildingType,
		OutBuildingData
	);
}


bool AIntermissionManager::TryPurchaseBuilding(
	EHubBuildingType BuildingType
)
{
	if (!GameInstanceRef)
	{
		return false;
	}

	FHubBuildingData FoundData;

	if (!GetBuildingData(BuildingType, FoundData))
	{
		return false;
	}

	if (!GameInstanceRef->PurchaseBuilding(FoundData))
	{
		return false;
	}

	const int32 NewLevel =
		GameInstanceRef->GetBuildingLevel(BuildingType);

	OnBuildingPurchased(BuildingType, NewLevel);

	return true;
}


void AIntermissionManager::ShowBuildingPreview(
	EHubBuildingType BuildingType
)
{
	FHubBuildingData FoundData;

	if (!GetBuildingData(BuildingType, FoundData))
	{
		return;
	}

	OnShowBuildingPreview(BuildingType, FoundData);
}


void AIntermissionManager::HideBuildingPreview()
{
	OnHideBuildingPreview();
}