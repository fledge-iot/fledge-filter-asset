/*
 * Fledge "asset" filter plugin rules classes.
 *
 * Copyright (c) 2025 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <rules.h>
#include <map>

using namespace std;
using namespace rapidjson;

/**
 * Constructor for the base rule class
 *
 * @param service The service name
 * @param asset	The asset name for the rule
 */
Rule::Rule(const string& service, const string& asset) : m_asset(asset),
	m_assetIsRegex(false), m_asset_re(NULL), m_service(service)
{
	m_logger = Logger::getLogger();
	if (isRegexString(asset))
	{
		try {
			m_asset_re = new regex(asset);
			m_assetIsRegex = true;
		} catch (...) {
			m_logger->error("Invalid regular expression for asset name '%s'.",
					asset.c_str());
		}
	}
	m_tracker = AssetTracker::getAssetTracker();
}

/**
 * Destructor for the base rule class
 */
Rule::~Rule()
{
	if (m_assetIsRegex)
		delete m_asset_re;
}

/**
 * Check if the rule should be run against the reading.
 * This is merely a case of checking if the asset name matches
 * in this instance.
 *
 * @param reading	The reading to match against the asset name
 */
bool Rule::match(Reading *reading)
{
	string assetName = reading->getAssetName();
	if (m_assetIsRegex)
		return regex_match(assetName, *m_asset_re);
	else if (assetName.compare(m_asset) == 0)
		return true;
	return false;
}

/**
 * Check to see if a string contains any special characters that would
 * show it is a regular expression rather than a simple name
 *
 * Once we find the first special character we return.
 *
 * @param str	The string to check
 * @return bool	Return true if any special characters are found
 */
bool Rule::isRegexString(const string& str)
{
	static const char *specials = ".*+[]()^$";
	for (const char *p = specials; *p; ++p)
		if (str.find(*p) != string::npos)
			return true;
	if (str.find("\\d") != string::npos)
		return true;
	return false;
}

/**
 * Constructor for the include rule
 *
 * @param service The service name
 * @param asset	The asset name
 */
IncludeRule::IncludeRule(const string& service, const string& asset) : Rule(service, asset)
{
}

/**
 * Default constructor for when this rule is the default
 * action of the plugin.
 */
IncludeRule::IncludeRule(const string& service) : Rule(service, "defaultAction")
{
}

/**
 * Destructor for the include rule
 */
IncludeRule::~IncludeRule()
{
	// Nothing to be done, the base class destructure 
	// will do all the work
}

/**
 * Execute the include rule
 *
 * Simply place the reading in the output vector
 *
 * @param reading	The reading to process
 * @param out		The vector in which to place the result
 */
void IncludeRule::execute(Reading *reading, vector<Reading *>& out)
{
	out.emplace_back(reading);
	if (m_tracker)
	{
		m_tracker->addAssetTrackingTuple(m_service, reading->getAssetName(), string("Filter"));
	}
}

/**
 * Constructor for the exclude rule
 *
 * @param service The service name
 * @param asset	The asset name
 */
ExcludeRule::ExcludeRule(const string& service, const string& asset) : Rule(service, asset)
{
}

/**
 * Default constructor for when this rule is the default
 * action of the plugin.
 */
ExcludeRule::ExcludeRule(const string& service) : Rule(service, "defaultAction")
{
}

/**
 * Destructor for the exclude rule
 */
ExcludeRule::~ExcludeRule()
{
	// Nothing to be done, the base class destructor 
	// will do all the work
}

/**
 * Execute the exclude rule
 *
 * Simply delete the reading and leave the output vector empty
 *
 * @param reading	The reading to process
 * @param out		The vector in which to place the result
 */
void ExcludeRule::execute(Reading *reading, vector<Reading *>& out)
{
	if (m_tracker)
	{
		m_tracker->addAssetTrackingTuple(m_service, reading->getAssetName(), string("Filter"));
	}
	delete reading;
}

/**
 * Constructor for the rename rule
 *
 * @param service The service name
 * @param asset	The asset name
 * @param json	JSON iterator
 */
RenameRule::RenameRule(const string& service, const string& asset, const Value& json) :
	Rule(service, asset), m_isRegex(false)
{
	if (json.HasMember("new_asset_name") && json["new_asset_name"].IsString())
	{
		m_newName = json["new_asset_name"].GetString();
		if (isRegexString(m_newName))
		{
			try {
				m_newRegex = new regex(m_newName);
				m_isRegex = true;
			} catch (...) {
				m_logger->error("Invalid regular expression '%s' for asset name '%s'",
						m_newName.c_str(), asset.c_str());
			}
		}
	}
	else
	{
		m_logger->error("Badly defined rename rule for asset '%s', a 'new_asset_name' property must be given and it must be a string.", m_asset.c_str());
	}
}

/**
 * Destructor for the rename rule
 */
RenameRule::~RenameRule()
{
	if (m_isRegex)
		delete m_newRegex;
}

/**
 * Execute the rename rule
 *
 * @param reading	The reading to process
 * @param out		The vector in which to place the result
 */
void RenameRule::execute(Reading *reading, vector<Reading *>& out)
{
	string origName = reading->getAssetName();
	if (!m_isRegex)
	{
		reading->setAssetName(m_newName);
	}
	else
	{
		string name = reading->getAssetName();
		reading->setAssetName(regex_replace(name, *m_asset_re, m_newName));
	}
	if (m_tracker)
	{
		m_tracker->addAssetTrackingTuple(m_service, origName,  string("Filter"));
		m_tracker->addAssetTrackingTuple(m_service, reading->getAssetName(), string("Filter"));
	}
	out.emplace_back(reading);
}

/**
 * Constructor for the datapoint map rule
 *
 * @param service The service name
 * @param asset	The asset name
 * @param json	JSON iterator
 */
DatapointMapRule::DatapointMapRule(const string& service, const string& asset, const Value& json) :
	Rule(service, asset)
{
	if (json.HasMember("map"))
	{
		const Value& map = json["map"];
		for (auto& mapit : map.GetObject())
		{
			string origName = mapit.name.GetString();
			if (mapit.value.IsString())
			{
				string newName = mapit.value.GetString();
				if (isRegexString(origName))
				{
					m_dpRegexMap.insert(pair<regex *, string>(new regex(origName), newName));
				}
				else
				{
					m_dpMap.insert(pair<string, string>(origName, newName));
				}
			}
			else
			{
				m_logger->error("The new name for the datapoint '%s' for assets '%s' must be a string.", m_asset.c_str(), origName.c_str());
			}
		}
	}
	else
	{
		m_logger->error("The 'datapointmap' rule must have a map item defined. The rule for asset '%s' will be ignored.", m_asset.c_str());
	}
}

/**
 * Destructor for the datapoint map rule
 */
DatapointMapRule::~DatapointMapRule()
{
	for (auto& regexes : m_dpRegexMap)
		delete regexes.first;
}

/**
 * Execute the datapoint map rule
 *
 * @param reading	The reading to process
 * @param out		The vector in which to place the result
 */
void DatapointMapRule::execute(Reading *reading, vector<Reading *>& out)
{

	// Iterate over the datapoints and change the names
	vector<Datapoint *> dps = reading->getReadingData();
	for (auto it = dps.begin(); it != dps.end(); ++it)
	{
		Datapoint *dp = *it;
		string name = dp->getName();
		auto i = m_dpMap.find(name);
		if (i != m_dpMap.end())
		{
			dp->setName(i->second);
		}
		else
		{
			for (auto& regexes : m_dpRegexMap)
			{
				if (regex_match(name, *regexes.first))
				{
					dp->setName(regex_replace(name, *regexes.first, regexes.second));
					break;
				}
			}
		}
	}
	if (m_tracker)
	{
		m_tracker->addAssetTrackingTuple(m_service, reading->getAssetName(), string("Filter"));
	}
	out.emplace_back(reading);
}
