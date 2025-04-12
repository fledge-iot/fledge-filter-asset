/*
 * Fledge "asset" filter plugin select rule class.
 *
 * Copyright (c) 2025 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <rules.h>
#include <map>
#include <set>

using namespace std;
using namespace rapidjson;

static const    set<string> validDpTypes{"FLOAT", "INTEGER", "STRING", "FLOAT_ARRAY", "DP_DICT", "DP_LIST", "IMAGE", "DATABUFFER", "2D_FLOAT_ARRAY", "NUMBER", "NON-NUMERIC", "NESTED", "ARRAY", "2D_ARRAY", "USER_ARRAY"};

/**
 * Constructor for the select map rule
 *
 * @param asset	The asset name
 * @param json	JSON iterator
 */
SelectRule::SelectRule(const string& asset, const Value& json) : Rule(asset)
{
	if (json.HasMember("type") && json["type"].IsString())
	{
		m_type = json["type"].GetString();
		transform(m_type.begin(), m_type.end(), m_type.begin(), ::toupper);
		if (m_type == "FLOATING")
			m_type = "FLOAT";
		if (m_type == "BUFFER")
			m_type = "DATABUFFER";
		if (m_type == "NESTED")
			m_type = "DP_DICT";
		if (m_type == "2D_ARRAY")
			m_type = "2D_FLOAT_ARRAY";
		if (m_type == "ARRAY")
			m_type = "FLOAT_ARRAY";

		if (!validateType(m_type))
		{
			m_logger->warn("Invalid Datapoint type %s given in select rule for asset '%s'. The rule will have no impact.", m_type.c_str(), m_asset.c_str());
		}
	}
	else if (json.HasMember("datapoints") && json["datapoints"].IsArray())
	{
		const Value& dps = json["datapoints"];
		for (auto& dp : dps.GetArray())
		{
			string dpName = dp.GetString();
			if (isRegexString(dpName))
				m_regexes.push_back(regex(dpName));
			else
				m_datapoints.push_back(dpName);
		}
	}
	else if (json.HasMember("datapoint") && json["datapoint"].IsString())
	{
		string dpName = json["datapoint"].GetString();
		if (isRegexString(dpName))
			m_regexes.push_back(regex(dpName));
		else
			m_datapoints.push_back(dpName);
	}
	else
	{
		m_logger->error("The select rule in the asset filter must have a datapoints item that is a list of datapoint names. The select rule for asset '%s' will be ignored.", asset.c_str());
	}
}

/**
 * Destructure for the select rule
 */
SelectRule::~SelectRule()
{
	// Nothing to be done, the base class destructure 
	// will do all the work
}

/**
 * Execute the map select rule.
 *
 * NB We first match agaisnt all the literal names and then,
 * if no match is foudn we try the regex names. This is faster
 * as regex is relatively slow. We always terminate on the first
 * match to improve performance.
 *
 * @param reading	The reading to process
 * @param out		The vector in which to place the result
 */
void SelectRule::execute(Reading *reading, vector<Reading *>& out)
{
	// Iterate over the datapoints and remove unwanted
	//Reading *newReading = new Reading(*reading); // copy original Reading object
	vector<Datapoint *>& dps = reading->getReadingData();
	vector<string> rmList;
	for (auto& dp : dps)
	{
		string name = dp->getName();
		bool found = false;
		if (!m_type.empty())
		{
			const DatapointValue dpv = dp->getData();
			string dpvStr = dpv.getTypeStr();
			transform(dpvStr.begin(), dpvStr.end(), dpvStr.begin(), ::toupper);
			if (dpvStr == m_type)
			{
				found = true;
			}
			else if (m_type == "NUMBER")
			{
				if (dpvStr == "FLOAT" || dpvStr == "INTEGER")
				{
					found = true;
				}
			}
			else if (m_type == "NON-NUMERIC")
			{
				if (dpvStr != "FLOAT" && dpvStr != "INTEGER")
				{
					found = true;
				}
			}
			else if (m_type == "USER_ARRAY")
			{
				if (dpvStr == "FLOAT_ARRAY" || dpvStr == "2D_FLOAT_ARRAY" )
				{
					found = true;
				}
			}
			if (!found)
			{
				rmList.push_back(name);
			}
		}
		else
		{
			for (auto& datapoint : m_datapoints)
			{
				if (datapoint.compare(name) == 0)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				// No literal matches found, now try the regex maatches
				for (auto& re : m_regexes)
				{
					if (regex_match(name, re))
					{
						found = true;
						break;
					}
				}
			}
			if (!found)
			{
				rmList.push_back(name);
			}
		}
	}
	for (auto name : rmList)
	{
		Datapoint *r =  reading->removeDatapoint(name);
		delete r;
	}
	if (reading->getDatapointCount() > 0)
		out.push_back(reading);
	else
		delete reading;
}

/**
 * Validate the type given in the configureation
 *
 * @param type	The type name to validate
 * @return bool	Return true if the type is valid
 */
bool SelectRule::validateType(const string& type)
{
	if (validDpTypes.find(type) != validDpTypes.end())
		return false;
	return true;
}

