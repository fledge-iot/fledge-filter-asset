/*
 * Fledge "asset" filter plugin nest rule.
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
 * Constructor for the nest map rule
 *
 * @param service The name of the service
 * @param asset	The asset name
 * @param json	JSON configuration element for rule
 */
NestRule::NestRule(const string& service, const string& asset, const Value& json) : Rule(service, asset)
{
	if (json.HasMember("nest"))
	{
		const Value& nest = json["nest"];
		if (!nest.IsObject())
		{
			m_logger->error( "The 'nest' property for asset %s is not a JSON object. The rule will be ignored.", m_asset.c_str() );
			return;
		}

		// Iterate over nest key
		for (auto itr2 = nest.MemberBegin(); itr2 != nest.MemberEnd(); itr2++)
		{
			vector<string> nestDataPoints;
			string newDatapointName = itr2->name.GetString();
			if (!itr2->value.IsArray())
			{
				m_logger->error( "The 'nest' rule for  asset %s does not have list of data points. The rule will be ignored.", asset.c_str());
				continue;
			}
			// Iterate over different nest asset datapoints list
			for (auto itr3 = itr2->value.Begin(); itr3 != itr2->value.End(); itr3++)
			{
				if (itr3->IsString())
				{
					string dpName =  itr3->GetString();
					nestDataPoints.push_back(dpName);
				}
				else
				{
					m_logger->error("Asset name in nest list for asset '%s' is not a string.", m_asset.c_str());
				}
			}
			// Populate current nest asset datapoints
			m_nest.insert(make_pair(newDatapointName,nestDataPoints));
		}
	}
}

/**
 * Destructor for NestRule
 */
NestRule::~NestRule()
{
	// Base class does all the work
}

/**
 * Execute the map Nest rule
 *
 * @param reading	The reading to process
 * @param out		The vector in which to place the result
 */
void NestRule::execute(Reading *reading, vector<Reading *>& out)
{
	vector<Datapoint *> dps = reading->getReadingData();

	if (!m_nest.empty())
	{
		// Iterate over nest assets
		for (auto const &pair: m_nest)
		{
			string newDatapointName = pair.first;
			vector<string> nestAssetDPs = pair.second;
			vector<Datapoint *> *newDatapoints = new vector<Datapoint *>;
			bool isDatapoint = false;

			// Iterate over nest assets datapoints
			for (string dpName : nestAssetDPs)
			{
				Datapoint *dp = reading->removeDatapoint(dpName);
				if (dp)
					newDatapoints->emplace_back(dp);
			}
			DatapointValue dpv(newDatapoints, true);
			Datapoint *dp = new Datapoint(newDatapointName, dpv);
			reading->addDatapoint(dp);
		}
	}
	if (m_tracker)
	{
		m_tracker->addAssetTrackingTuple(m_service, reading->getAssetName(), string("Filter"));
	}
	out.emplace_back(reading);
}

