/*
 * Fledge "asset" filter plugin  remove rule
 *
 * Copyright (c) 2025 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <logger.h>
#include <rules.h>
#include <set>

using namespace std;
using namespace rapidjson;

static const    set<string> validDpTypes{"FLOAT", "INTEGER", "STRING", "FLOAT_ARRAY", "DP_DICT", "DP_LIST", "IMAGE", "DATABUFFER", "2D_FLOAT_ARRAY", "NUMBER", "NON-NUMERIC", "NESTED", "ARRAY", "2D_ARRAY", "USER_ARRAY"};

/**
 * Constructor for the remove rule
 *
 * @param asset	The asset name
 * @param json	JSON object
 */
RemoveRule::RemoveRule(const string& asset, const rapidjson::Value& json) : Rule(asset), m_regex(NULL)
{
	if (json.HasMember("datapoint") && json["datapoint"].IsString())
	{
		string datapoint = json["datapoint"].GetString();
		if (isRegexString(datapoint))
			m_regex = new regex(datapoint);
		else
			m_datapoint = datapoint;
	}
	else if (json.HasMember("type") && json["type"].IsString())
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
			m_logger->warn("Invalid Datapoint type %s given in rule for asset '%s'. The rule will have no impact.", m_type.c_str(), m_asset.c_str());
		}
	}
	else if (json.HasMember("datapoints") && json["datapoints"].IsArray())
	{
		const Value& dps = json["datapoints"];
		for (auto& dp : dps.GetArray())
		{
			if (dp.IsString())
				m_datapoints.push_back(dp.GetString());
			else
				m_logger->error("The datapoints in the array of names for the asset '%s' must all be strings.", m_asset.c_str());
		}
	}
	else
	{
		m_logger->error("Badly defined remove rule for asset '%s'. A 'datapoint', 'type' or 'datapoints' property must be given. The 'datapoint' and 'type' properties must be strings and 'datapopints' is expected to be an array of strings.", m_asset.c_str());

	}
}

/**
 * Destructor for the remove rule
 */
RemoveRule::~RemoveRule()
{
	if (m_regex)
		delete m_regex;
}

/**
 * Execute the remove rule
 *
 * @param reading	The reading to process
 * @param out		The vector in which to place the result
 */
void RemoveRule::execute(Reading *reading, vector<Reading *>& out)
{
	vector<Datapoint *>& dps = reading->getReadingData();
	for (auto it = dps.begin(); it != dps.end(); )
	{
		Datapoint *dp = *it;
		const DatapointValue dpv = dp->getData();
		string dpvStr = dpv.getTypeStr();
		if (!m_datapoint.empty())
		{
			if (m_datapoint.compare(dp->getName()) == 0)
			{
				it = dps.erase(it);
				m_logger->debug("Removing datapoint with name %s", dp->getName().c_str());
				delete dp;
			}
			else
				++it;
		}
		else if (m_regex)
		{
			if (regex_match(dp->getName(), *m_regex))
			{
				it = dps.erase(it);
				m_logger->debug("Removing datapoint with name %s", dp->getName().c_str());
				delete dp;
			}
			else
				++it;
		}
		else if (!m_type.empty())
		{
			const DatapointValue dpv = dp->getData();
			string dpvStr = dpv.getTypeStr();
			if (dpvStr == m_type)
			{
				it = dps.erase(it);
				m_logger->debug("Removing datapoint with type %s", m_type.c_str());
				delete dp;
			}
			else if (m_type == "NUMBER")
			{
				if (dpvStr == "FLOAT" || dpvStr == "INTEGER")
				{
					it = dps.erase(it);
					delete dp;
					m_logger->debug("Removing datapoint with type %s", dpvStr.c_str());
				}
				else 
					++it;
			}
			else if (m_type == "NON-NUMERIC")
			{
				if (dpvStr != "FLOAT" && dpvStr != "INTEGER")
				{
					it = dps.erase(it);
					m_logger->debug("Removing datapoint with type %s", dpvStr.c_str());
					delete dp;
				}
				else
					++it;
			}
			else if (m_type == "USER_ARRAY")
			{
				if (dpvStr == "FLOAT_ARRAY" || dpvStr == "2D_FLOAT_ARRAY" )
				{
					it = dps.erase(it);
					m_logger->debug("Removing datapoint with type %s", dpvStr.c_str());
					delete dp;
				}
				else
					++it;
			}
			else
				++it;
		}
		else if (!m_datapoints.empty())
		{
			bool removed = false;
			for (auto& name : m_datapoints)
			{
				if (isRegexString(name))
				{
					regex regexPattern(name);
					if (regex_match(dp->getName(), regexPattern))
					{
						it = dps.erase(it);
						m_logger->debug("Removing datapoint with name %s", dp->getName().c_str());
						delete dp;
						removed = true;
					}
				}
				else if (name.compare(dp->getName()) == 0)
				{
					it = dps.erase(it);
					m_logger->debug("Removing datapoint with name %s", dp->getName().c_str());
					delete dp;
					removed = true;
				}
			}
			if (!removed)
			{
				++it;
			}
		}
		else
			++it;
	}
	out.emplace_back(reading);
}

/**
 * Validate the type given in the configuration
 *
 * @param type	The type name to validate
 * @return bool	Return true if the type is valid
 */
bool RemoveRule::validateType(const string& type)
{
	if (validDpTypes.find(type) != validDpTypes.end())
		return false;
	return true;
}

