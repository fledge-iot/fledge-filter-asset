/*
 * Fledge "asset" filter plugin flatten rule.
 *
 * Copyright (c) 2025 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <rules.h>

using namespace std;

/**
 * Default constructor for the Flatten rule
 *
 * @param asset	The asset name
 */
FlattenRule::FlattenRule() : Rule("defaultAction")
{
}

/**
 * Constructor for the Flatten rule
 *
 * @param asset	The asset name
 */
FlattenRule::FlattenRule(const string& asset) : Rule(asset)
{
}

/**
 * Detructor for flatten rule
 */
FlattenRule::~FlattenRule()
{
	// Base destructor does all the work
}

/**
 * Execute the Flatten rule
 *
 * @param reading	The reading to process
 * @param out		The vector in which to place the result
 */
void FlattenRule::execute(Reading *reading, vector<Reading *>& out)
{

	// Iterate over the datapoints and change the names
	vector<Datapoint *> dps = reading->getReadingData();
	vector<Datapoint *> flattenDatapoints;
	for (auto it = dps.begin(); it != dps.end(); ++it)
	{
		Datapoint *dp = new  Datapoint(**it);
		const DatapointValue dpv = dp->getData();
		if (dpv.getType() == DatapointValue::T_DP_DICT || dpv.getType() == DatapointValue::T_DP_LIST)
		{
			flattenDatapoint(dp, dp->getName(), flattenDatapoints);
			delete dp;
		}
		else
		{
			flattenDatapoints.emplace_back(dp);
		}
	}
	// Create a new reading with the flattened datapoints. Maintain the timestamps
	// from the old reading
	Reading *newReading (new Reading(reading->getAssetName(), flattenDatapoints,
				reading->getAssetDateTime()));
	struct timeval tm;
	reading->getUserTimestamp(&tm);
	newReading->setUserTimestamp(tm);
	delete reading;
	out.emplace_back(newReading);
}

/**
 * Flatten nested datapoint
 *
 * @param datapoint		datapoint to flatten
 * @param datapointName	Name of datapoint
 * @param flattenDatapoints	vector of flattened datapoints
 */

void FlattenRule::flattenDatapoint(Datapoint *datapoint,  string datapointName, vector<Datapoint *>& flattenDatapoints)
{
	DatapointValue datapointValue = datapoint->getData();

	vector<Datapoint *> *&nestedDP = datapointValue.getDpVec();

	for (auto dpit = nestedDP->begin(); dpit != nestedDP->end(); dpit++)
	{

		string name = (*dpit)->getName();
		DatapointValue val = (*dpit)->getData();
		
		if (val.getType() == DatapointValue::T_DP_DICT || val.getType() == DatapointValue::T_DP_LIST)
		{
			datapointName = datapointName + "_" + name;
			flattenDatapoint(*dpit, datapointName, flattenDatapoints);
		}
		else
		{
			name = datapointName + "_" + name;
			flattenDatapoints.emplace_back(new Datapoint(name, val) );	
		}

	}
	
}

