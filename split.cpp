/*
 * Fledge "asset" filter plugin split rule.
 *
 * Copyright (c) 2025 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <rules.h>
#include <asset_tracking.h>

using namespace std;
using namespace rapidjson;

/**
 * Constructor for the split map rule
 *
 * @param service	The name of the service
 * @param asset	The asset name
 * @param json	JSON iterator
 */
SplitRule::SplitRule(const string& service, const string& asset, const Value& json) : Rule(service, asset)
{
	string newAssetName = {};

	if (json.HasMember("split"))
	{
		const Value& split = json["split"];
		if (!split.IsObject())
		{
			m_logger->error( "The 'split' property for asset %s is not a JSON object. The rule will be ignored.", m_asset.c_str() );
			return;
		}

		// Iterate over split key
		for (auto itr2 = split.MemberBegin(); itr2 != split.MemberEnd(); itr2++)
		{
			vector<string> splitAssetDataPoints;
			splitAssetDataPoints.clear();
			newAssetName = itr2->name.GetString();
			if (!itr2->value.IsArray())
			{
				m_logger->error( "The 'split' rule for  asset %s does not have list of data points. The rule will be ignored.", newAssetName.c_str());
				continue;
			}
			// Iterate over different split asset datapoints list
			for (auto itr3 = itr2->value.Begin(); itr3 != itr2->value.End(); itr3++)
			{
				if (itr3->IsString())
				{
					string dpName =  itr3->GetString();
					splitAssetDataPoints.push_back(dpName);
				}
				else
				{
					m_logger->error("Asset name in split list for asset '%s' is not a string.", m_asset.c_str());
				}
			}
			// Populate current split asset datapoints
			m_split.insert(make_pair(newAssetName,splitAssetDataPoints));
		}
	}
}

/**
 * Destructure for SplitRule
 */
SplitRule::~SplitRule()
{
	// Base class does all the work
}

/**
 * Execute the map Split rule
 *
 * @param reading	The reading to process
 * @param out		The vector in which to place the result
 */
void SplitRule::execute(Reading *reading, vector<Reading *>& out)
{
	AssetTracker *tracker = AssetTracker::getAssetTracker();
	vector<Datapoint *> dps = reading->getReadingData();

	// split key exists
	if (!m_split.empty())
	{
		// Iterate over split assets
		for (auto const &pair: m_split)
		{
			string newAssetName = pair.first;
			if (m_assetIsRegex)
				newAssetName = regex_replace(reading->getAssetName(), *m_asset_re, pair.first);
			vector<string> splitAssetDPs = pair.second;
			vector<Datapoint *> newDatapoints;
			bool isDatapoint = false;

			// Iterate over split assets datapoints
			for (string dpName : splitAssetDPs)
			{
				for (auto it = dps.begin(); it != dps.end(); it++ )
				{
					if (dpName == (*it)->getName())
					{
						Datapoint *dp = new  Datapoint(**it);
						newDatapoints.emplace_back(dp);
						isDatapoint = true;
					}
				}
			}
			if(isDatapoint)
			{
				// Add new asset to reading set and asset tracker
				out.emplace_back(new Reading(newAssetName, newDatapoints));
				if (tracker)
				{
					tracker->addAssetTrackingTuple(m_service, newAssetName, string("Filter"));
				}
			}
		}
	}
	else // Split key doesn't exist
	{
		// Iterate over the datapoints
		for (auto it = dps.begin(); it != dps.end(); it++)
		{
			string newAssetName = reading->getAssetName();
			newAssetName = newAssetName + "_" + (*it)->getName();
			Datapoint *dp = new  Datapoint(**it);

			// Add new asset to reading set and asset tracker
			out.emplace_back(new Reading(newAssetName, dp));
			if (tracker)
			{
				tracker->addAssetTrackingTuple(m_service, newAssetName, string("Filter"));
			}
		}
	}
	delete reading;
}

