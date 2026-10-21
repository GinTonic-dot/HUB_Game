#include "HubGameInstance.h"

int32 UHubGameInstance::GetBuildingLevel(EHubBuildingType BuildingType) const
{
	if (const int32* FoundLevel = BuildingLevels.Find(BuildingType))
	{
		return *FoundLevel;
	}

	return 0;
}

int32 UHubGameInstance::GetBuildingCost(const FHubBuildingData& InBuildingData) const
{
	const int32 CurrentLevel =
		GetBuildingLevel(InBuildingData.BuildingType);

	return InBuildingData.BaseCost
		+ (CurrentLevel * InBuildingData.CostIncreasePerLevel);
}


bool UHubGameInstance::CanPurchaseBuilding(const FHubBuildingData& InBuildingData) const
{
	const int32 CurrentLevel =
		GetBuildingLevel(InBuildingData.BuildingType);

	if (CurrentLevel >= InBuildingData.MaxLevel)
	{
		return false;
	}

	const int32 Cost = GetBuildingCost(InBuildingData);

	return TotalResources >= Cost;
}

bool UHubGameInstance::PurchaseBuilding(const FHubBuildingData& InBuildingData)
{
	if (!CanPurchaseBuilding(InBuildingData))
	{
		return false;
	}

	const int32 Cost = GetBuildingCost(InBuildingData);

	const int32 CurrentLevel =
		GetBuildingLevel(InBuildingData.BuildingType);

	TotalResources -= Cost;

	BuildingLevels.FindOrAdd(InBuildingData.BuildingType)
		= CurrentLevel + 1;

	return true;
}

bool UHubGameInstance::GetBuildingData(
	EHubBuildingType BuildingType,
	FHubBuildingData& OutBuildingData
) const
{
	for (const FHubBuildingData& Entry : BuildingData)
	{
		if (Entry.BuildingType == BuildingType)
		{
			OutBuildingData = Entry;
			return true;
		}
	}

	return false;
}