/*
 * Fledge "asset" filter plugin.
 *
 * Copyright (c) 2018 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Amandeep Singh Arora
 */

#include <string>
#include <iostream>
#include <plugin_api.h>
#include <config_category.h>
#include <filter.h>
#include <reading_set.h>
#include <version.h>
#include <algorithm>
#include <set>
#include <mutex>

#define RULES "\\\"rules\\\" : []"
#define FILTER_NAME "asset"


#define DEFAULT_CONFIG "{\"plugin\" : { \"description\" : \"Asset filter plugin\", " \
                       		"\"type\" : \"string\", " \
				"\"default\" : \"" FILTER_NAME "\", \"readonly\" : \"true\" }, " \
			 "\"enable\": {\"description\": \"A switch that can be used to enable or disable execution of " \
					 "the asset filter.\", " \
				"\"displayName\": \"Enabled\", " \
				"\"type\": \"boolean\", " \
				"\"default\": \"false\" }, " \
			"\"config\" : {\"description\" : \"JSON document that defines the rules for asset names.\", " \
				"\"type\" : \"JSON\", " \
				"\"default\" : \"{" RULES "}\", " \
				"\"order\" : \"1\", \"displayName\" : \"Asset rules\"} }"

using namespace std;

/**
 * The Filter plugin interface
 */
extern "C" {

/**
 * The plugin information structure
 */
static PLUGIN_INFORMATION info = {
        FILTER_NAME,              // Name
        VERSION,                  // Version
        0,                        // Flags
        PLUGIN_TYPE_FILTER,       // Type
        "1.0.0",                  // Interface version
	DEFAULT_CONFIG	          // Default plugin configuration
};

typedef enum
{
	INCLUDE,
	EXCLUDE,
	RENAME,
	REMOVE,
	FLATTEN,
	DPMAP,
	SPLIT
} action;

struct AssetAction {
	action actn;
	string new_asset_name; // valid only in case of RENAME
	map<string, string> dpmap;
	string datapoint; // valid only in case of REMOVE
	string type; // valid only in case of REMOVE
	map<string, map< string, vector<string> > > split_assets; // valid only in case of SPLIT
};

typedef struct
{
	FledgeFilter *handle;
	std::map<std::string, AssetAction> *assetFilterConfig;
	AssetAction	defaultAction;
	std::string	configCatName;
	std::mutex 	mutex;
} FILTER_INFO;

void splitAssetConfigure(Value &rules, map<string, map<string,vector<string>>> &splitAssets);
/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info()
{
	return &info;
}

/**
 * Initialise the plugin, called to get the plugin handle and setup the
 * output handle that will be passed to the output stream. The output stream
 * is merely a function pointer that is called with the output handle and
 * the new set of readings generated by the plugin.
 *     (*output)(outHandle, readings);
 * Note that the plugin may not call the output stream if the result of
 * the filtering is that no readings are to be sent onwards in the chain.
 * This allows the plugin to discard data or to buffer it for aggregation
 * with data that follows in subsequent calls
 *
 * @param config	The configuration category for the filter
 * @param outHandle	A handle that will be passed to the output stream
 * @param output	The output stream (function pointer) to which data is passed
 * @return		An opaque handle that is used in all subsequent calls to the plugin
 */
PLUGIN_HANDLE plugin_init(ConfigCategory* config,
			  OUTPUT_HANDLE *outHandle,
			  OUTPUT_STREAM output)
{

	FILTER_INFO *info = new FILTER_INFO;
	lock_guard<mutex> guard(info->mutex);
	info->handle = new FledgeFilter(FILTER_NAME, *config, outHandle, output);
	FledgeFilter *filter = info->handle;
	info->configCatName = config->getName();
	
	// Handle filter configuration
	info->assetFilterConfig = new std::map<std::string, AssetAction>;
	if (filter->getConfig().itemExists("config"))
	{
		Document document;

		if (document.Parse(filter->getConfig().getValue("config").c_str()).HasParseError())
		{
			Logger::getLogger()->error("Unable to parse filter config: '%s'", filter->getConfig().getValue("config").c_str());
			return NULL;
		}
		
		info->defaultAction = {action::INCLUDE, ""};
		Value::MemberIterator defaultAction = document.FindMember("defaultAction");
	    if(defaultAction == document.MemberEnd() || !defaultAction->value.IsString())
		{
			info->defaultAction.actn = action::INCLUDE;
			Logger::getLogger()->info("Parse asset filter config, unable to find defaultAction value: '%s', assuming 'include' for unmentioned asset names", filter->getConfig().getValue("config").c_str());
		}
		else
		{
			string actionStr= defaultAction->value.GetString();
			for (auto & c: actionStr) c = tolower(c);
			action actn;
			if (actionStr == "include")
				info->defaultAction.actn = action::INCLUDE;
			else if (actionStr == "exclude")
				info->defaultAction.actn = action::EXCLUDE;
			else if (actionStr == "flatten")
				info->defaultAction.actn = action::FLATTEN;
			else
			{
				Logger::getLogger()->error("Parse asset filter config, unable to parse defaultAction value: '%s'", filter->getConfig().getValue("config").c_str());
				return NULL;
			}
			Logger::getLogger()->info("Parse asset filter config, default action for unmentioned asset names=%d", info->defaultAction.actn);
		}

		// Check for "rules" main property
		if (!document.HasMember("rules"))
		{
			Logger::getLogger()->error("Parse asset filter config, unable to parse "
						   "the 'rules' top level property in '%s'",
						   filter->getConfig().getValue("config").c_str());
			return NULL;
		}
		Value &rules = document["rules"];
		if (!rules.IsArray())
		{
			Logger::getLogger()->error("Parse asset filter config, rules array is missing : '%s'", filter->getConfig().getValue("config").c_str());
			return NULL;
		}
		for (Value::ConstValueIterator iter = rules.Begin(); iter != rules.End(); ++iter)
		{
			if (!iter->IsObject())
			{
				Logger::getLogger()->error("Parse asset filter config, each entry in rules array must be an object: '%s'", filter->getConfig().getValue("config").c_str());
				return NULL;
			}
			if (!iter->HasMember("asset_name") || !iter->HasMember("action"))
			{
				Logger::getLogger()->error("Parse asset filter config, asset_name/action is not found in the entry: '%s'", filter->getConfig().getValue("config").c_str());
				return NULL;
			}
			
			string asset_name = (*iter)["asset_name"].GetString();
			string actionStr= (*iter)["action"].GetString();
			string new_asset_name = "";
			map<string, string> dpmap;
			map<string, map<string,vector<string>>> splitAssets;
			string datapoint;
			string dpType;
			for (auto & c: actionStr) c = tolower(c);
			action actn;
			if (actionStr == "include")
				actn = action::INCLUDE;
			else if (actionStr == "exclude")
				actn = action::EXCLUDE;
			else if (actionStr == "rename")
			{
				actn = action::RENAME;
				if (iter->HasMember("new_asset_name"))
					new_asset_name = (*iter)["new_asset_name"].GetString();
				else
				{
					Logger::getLogger()->error("Parse asset filter config, new_asset_name is not found in the RENAME entry: '%s'", filter->getConfig().getValue("config").c_str());
					return NULL;
				}
			}
			else if (actionStr == "datapointmap")
			{
				actn = action::DPMAP;
				if (iter->HasMember("map"))
				{
					const Value& map = (*iter)["map"];
					for (auto& mapit : map.GetObject())
						dpmap.insert(pair<string, string>(mapit.name.GetString(), mapit.value.GetString()));
				}
				else
				{
					Logger::getLogger()->error("Parse asset filter config, map is not found in the DatapointMap entry: '%s'", filter->getConfig().getValue("config").c_str());
					return NULL;
				}
			}
			else if (actionStr == "remove")
			{
				actn = action::REMOVE;

                                if (iter->HasMember("datapoint"))
				{
					if ((*iter)["datapoint"].IsString())
					{
                                        	datapoint = (*iter)["datapoint"].GetString();
					}
				}
                                if (iter->HasMember("type"))
				{
					if ((*iter)["type"].IsString())
					{
                                        	dpType = (*iter)["type"].GetString();
					}
				}
			}
			else if (actionStr == "flatten")
			{
				actn = action::FLATTEN;
			}
			else if (actionStr == "split")
			{
				actn = action::SPLIT;
				splitAssetConfigure(document["rules"], splitAssets);
			}
			Logger::getLogger()->info("Parse asset filter config, Adding to assetFilterConfig map: {%s, %d, %s}", asset_name.c_str(), actn, new_asset_name.c_str());
			(*info->assetFilterConfig)[asset_name] = {actn, new_asset_name, dpmap, datapoint, dpType, splitAssets};
		}
	}
	else
	{
		Logger::getLogger()->error("No config provided for asset filter, cannot proceed");
		return NULL;
	}
	
	return (PLUGIN_HANDLE)info;
}

/**
 * splitAssetConfigure populate splitAssets parameter from rules JSON for split action
 *
 * @param rules		rules JSON
 * @param splitAssets	Container to be populated with split asset name and datapoints
 */

void splitAssetConfigure(Value &rules, map<string, map<string,vector<string>>> &splitAssets)
{
	for (auto itr = rules.Begin(); itr != rules.End(); ++itr)
	{
		string newAssetName = {};
		string asset_name = (*itr)["asset_name"].GetString();
		// split key exists
		if (itr->HasMember("split"))
		{
			if (!(*itr)["split"].IsObject())
			{
				Logger::getLogger()->error( "split key for asset %s is not an object", asset_name.c_str() );
				continue;
			}

			map<string, vector<string>> newSplitAsset;
			// Iterate over split key
			for (auto itr2 = (*itr)["split"].MemberBegin(); itr2 != (*itr)["split"].MemberEnd(); itr2++)
			{
				vector<string> splitAssetDataPoints;
				splitAssetDataPoints.clear();
				newAssetName = itr2->name.GetString();
				if (!itr2->value.IsArray())
				{
					Logger::getLogger()->error( "split asset %s does not have list of data points", newAssetName.c_str());
					continue;
				}
				// Iterate over different split asset datapoints list
				for (auto itr3 = itr2->value.Begin(); itr3 != itr2->value.End(); itr3++)
				{
					string dpName =  itr3->GetString();
					splitAssetDataPoints.push_back(dpName);
				}
				// Populate current split asset datapoints
				newSplitAsset.insert(std::make_pair(newAssetName,splitAssetDataPoints));
			}
			// Populate split asset data for configured asset
			splitAssets.insert(std::make_pair(asset_name,newSplitAsset));
		}
		else
		{
			// Populate split asset data for configured asset
			map<string, vector<string>> newSplitAsset;
			splitAssets.insert(std::make_pair(asset_name,newSplitAsset));
		}
	}
}

/**
 * Flatten nested datapoint
 *
 * @param datapoint		datapoint to flatten
 * @param datapointName	Name of datapoint
 * @param flattenDatapoints	vector of flatten datapoints
 */

void flattenDatapoint(Datapoint *datapoint,  string datapointName, vector<Datapoint *>& flattenDatapoints)
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

/**
 * Ingest a set of readings into the plugin for processing
 *
 * @param handle	The plugin handle returned from plugin_init
 * @param readingSet	The readings to process
 */
void plugin_ingest(PLUGIN_HANDLE *handle,
		   READINGSET *readingSet)
{
	FILTER_INFO *info = (FILTER_INFO *) handle;
	lock_guard<mutex> guard(info->mutex);
	FledgeFilter* filter = info->handle;
	
	if (!filter->isEnabled())
	{
		// Current filter is not active: just pass the readings set
		filter->m_func(filter->m_data, readingSet);
		return;
	}

	ReadingSet *origReadingSet = (ReadingSet *) readingSet;
	vector<Reading *> newReadings;

	// Just get all the readings in the readingset
	const vector<Reading *>& readings = origReadingSet->getAllReadings();

	static const std::set<std::string> validDpTypes{"FLOAT", "INTEGER", "STRING", "FLOAT_ARRAY", "DP_DICT", "DP_LIST", "IMAGE", "DATABUFFER", "2D_FLOAT_ARRAY", "NUMBER", "NON-NUMERIC", "NESTED", "ARRAY", "2D_ARRAY", "USER_ARRAY"};
	
	// Iterate the readings
	for (vector<Reading *>::const_iterator elem = readings.begin();
						      elem != readings.end();
						      ++elem)
	{
		std::map<std::string, AssetAction>::iterator it = info->assetFilterConfig->find((*elem)->getAssetName());
		AssetAction *assetAction;
		if (it == info->assetFilterConfig->end())
		{
			//Logger::getLogger()->info("Unable to find asset_name '%s' in map, using default action %d", (*elem)->getAssetName().c_str(), info->defaultAction.actn);
			assetAction = &(info->defaultAction);
		}
		else
		{
			assetAction = &((*info->assetFilterConfig)[(*elem)->getAssetName()]);
			//Logger::getLogger()->info("Reading's asset_name=%s, matching map entry={%s, %d, %s}", 
				//	(*elem)->getAssetName().c_str(), (*elem)->getAssetName().c_str(), assetAction->actn, assetAction->new_asset_name.c_str());
		}

		if(assetAction->actn == action::INCLUDE)
		{
			newReadings.push_back(new Reading(**elem)); // copy original Reading object
			AssetTracker *tracker = AssetTracker::getAssetTracker();
			if (tracker)
			{
				tracker->addAssetTrackingTuple(info->configCatName, (*elem)->getAssetName(), string("Filter"));
			}
		}
		else if(assetAction->actn == action::EXCLUDE)
		{
			// no need to free memory allocated for original reading object: done in ReadingSet destructor
			AssetTracker *tracker = AssetTracker::getAssetTracker();
			if (tracker)
			{
				tracker->addAssetTrackingTuple(info->configCatName, (*elem)->getAssetName(), string("Filter"));
			}
		}
		else if(assetAction->actn == action::RENAME)
		{
			Reading *newRdng = new Reading(**elem); // copy original Reading object
			newRdng->setAssetName(assetAction->new_asset_name);
			newReadings.push_back(newRdng);
			AssetTracker *tracker = AssetTracker::getAssetTracker();
			if (tracker)
			{
				tracker->addAssetTrackingTuple(info->configCatName, (*elem)->getAssetName(), string("Filter"));
				tracker->addAssetTrackingTuple(info->configCatName, assetAction->new_asset_name, string("Filter"));
			}
		}
		else if (assetAction->actn == action::DPMAP)
		{
			Reading *newReading = new Reading(**elem); // copy original Reading object
			// Iterate over the datapoints and change the names
			vector<Datapoint *> dps = newReading->getReadingData();
			for (auto it = dps.begin(); it != dps.end(); ++it)
			{
				Datapoint *dp = *it;
				auto i = assetAction->dpmap.find(dp->getName());
				if (i != assetAction->dpmap.end())
				{
					dp->setName(i->second);
				}
			}
			newReadings.push_back(newReading);
		}
		else if (assetAction->actn == action::REMOVE)
		{
			Reading *newReading = new Reading(**elem); // copy original Reading object
                        // Iterate over the datapoints and change the names
                        vector<Datapoint *>& dps = newReading->getReadingData();
			bool dpFound = false;
			bool typeFound = false;
			string datapoint;
			string type;
                        for (auto it = dps.begin(); it != dps.end(); )
                        {
                                Datapoint *dp = *it;
                                datapoint = assetAction->datapoint;
				type = assetAction->type;
				const DatapointValue dpv = dp->getData();
                                string dpvStr = dpv.getTypeStr();
                                if (!datapoint.empty())
                                {
                                	if (datapoint == dp->getName())
					{
						it = dps.erase(it);
						Logger::getLogger()->info("Removing datapoint with name %s", datapoint.c_str());
						dpFound = true;
					}
					else
						++it;
                                }
				else if (!type.empty())
				{
					transform(type.begin(), type.end(), type.begin(), ::toupper);
					if (type == "FLOATING") type = "FLOAT";
					if (type == "BUFFER") type = "DATABUFFER";
					if (type == "NESTED") type = "DP_DICT";
					if (type == "2D_ARRAY") type = "2D_FLOAT_ARRAY";
					if (type == "ARRAY") type = "FLOAT_ARRAY";

					if (validDpTypes.find (type) == validDpTypes.end())
					{
						Logger::getLogger()->warn("Invalid Datapoint type %s", type.c_str());
						break;
					}
					const DatapointValue dpv = dp->getData();
					string dpvStr = dpv.getTypeStr();
					if (dpvStr == type)
					{
						it = dps.erase(it);
						Logger::getLogger()->info("Removing datapoint with type %s", type.c_str());
						typeFound = true;
					}
					else if (type == "NUMBER")
                                        {
                                                if (dpvStr == "FLOAT" || dpvStr == "INTEGER")
                                                {
                                                        it = dps.erase(it);
                                                        Logger::getLogger()->info("Removing datapoint with type %s", dpvStr.c_str());
                                                        typeFound = true;
                                                }
						else 
							++it;
                                        }
                                        else if (type == "NON-NUMERIC")
                                        {
                                                if (dpvStr != "FLOAT" && dpvStr != "INTEGER")
                                                {
                                                        it = dps.erase(it);
                                                        Logger::getLogger()->info("Removing datapoint with type %s", dpvStr.c_str());
                                                        typeFound = true;
                                                }
						else
							++it;
                                        }
					else if (type == "USER_ARRAY")
					{
						if (dpvStr == "FLOAT_ARRAY" || dpvStr == "2D_FLOAT_ARRAY" )
                                                {
                                                        it = dps.erase(it);
                                                        Logger::getLogger()->info("Removing datapoint with type %s", dpvStr.c_str());
                                                        typeFound = true;
                                                }
                                                else
                                                        ++it;
					}
					else
						++it;
				}
				else
					++it;
                        }
			if (!datapoint.empty() && !dpFound)
				Logger::getLogger()->info("Datapoint with name %s not found for removal", datapoint.c_str());
			if (!type.empty() && !typeFound)
				Logger::getLogger()->info("Datapoint with type %s not found for removal", type.c_str());

                        newReadings.push_back(newReading);
		}
		else if (assetAction->actn == action::FLATTEN)
		{
			// Iterate over the datapoints and change the names
			vector<Datapoint *> dps = (*elem)->getReadingData();
			vector<Datapoint *> flattenDatapoints;
			for (auto it = dps.begin(); it != dps.end(); ++it)
			{
				Datapoint *dp = new  Datapoint(**it);
				const DatapointValue dpv = dp->getData();
				if (dpv.getType() == DatapointValue::T_DP_DICT || dpv.getType() == DatapointValue::T_DP_LIST)
				{
					flattenDatapoint(dp, dp->getName(), flattenDatapoints);
				}
				else
				{
					flattenDatapoints.emplace_back(dp);
				}

				newReadings.emplace_back(new Reading((*elem)->getAssetName(), flattenDatapoints));
			}

		}
		else if (assetAction->actn == action::SPLIT)
		{
			auto found = assetAction->split_assets.find((*elem)->getAssetName());
			if (found != assetAction->split_assets.end())
			{
				AssetTracker *tracker = AssetTracker::getAssetTracker();
				vector<Datapoint *> dps = (*elem)->getReadingData();
				auto splitAssets = found->second;

				// split key exists
				if (!splitAssets.empty())
				{
					// Iterate over split assets
					for (auto const &pair: found->second)
					{
						std::string newAssetName = pair.first;
						vector<string> splitAssetDPs = pair.second;
						std::vector<Datapoint *> newDatapoints;
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
							newReadings.emplace_back(new Reading(newAssetName, newDatapoints));
							if (tracker)
							{
								tracker->addAssetTrackingTuple(info->configCatName, newAssetName, string("Filter"));
							}
						}
					}
				}
				else // Split key doesn't exist
				{
					// Iterate over the datapoints
					for (auto it = dps.begin(); it != dps.end(); it++)
					{
						std::string newAssetName = (*elem)->getAssetName();
						newAssetName = newAssetName + "_" + (*it)->getName();
						Datapoint *dp = new  Datapoint(**it);

						// Add new asset to reading set and asset tracker
						newReadings.emplace_back(new Reading(newAssetName, dp));
						if (tracker)
						{
							tracker->addAssetTrackingTuple(info->configCatName, newAssetName, string("Filter"));
						}
					}
				}
			}
		}
	}

	delete (ReadingSet *)readingSet;

	ReadingSet *newReadingSet = new ReadingSet(&newReadings);	
	filter->m_func(filter->m_data, newReadingSet);
}

/**
 * Plugin reconfiguration entry point
 *
 * @param	handle	The plugin handle
 * @param	newConfig	The new configuration data
 */
void plugin_reconfigure(PLUGIN_HANDLE *handle, const string& newConfig)
{

	FILTER_INFO *info = (FILTER_INFO *) handle;
	lock_guard<mutex> guard(info->mutex);
	FledgeFilter* filter = info->handle;

	filter->setConfig(newConfig);

	if (filter->getConfig().itemExists("config"))
	{
		FILTER_INFO *newInfo = new FILTER_INFO;
		newInfo->assetFilterConfig = new std::map<std::string, AssetAction>;
		Document	document;
		if (document.Parse(filter->getConfig().getValue("config").c_str()).HasParseError())
		{
			Logger::getLogger()->error("Unable to parse filter config: '%s'", filter->getConfig().getValue("config").c_str());
			return;
		}
		
		newInfo->defaultAction = {action::INCLUDE, ""};
		Value::MemberIterator defaultAction = document.FindMember("defaultAction");
	    if(defaultAction == document.MemberEnd() || !defaultAction->value.IsString())
		{
			newInfo->defaultAction.actn = action::INCLUDE;
			Logger::getLogger()->info("Parse asset filter config, unable to find defaultAction value: '%s', assuming 'include' for unmentioned asset names", filter->getConfig().getValue("config").c_str());
		}
		else
		{
			string actionStr= defaultAction->value.GetString();
			for (auto & c: actionStr) c = tolower(c);
			action actn;
			if (actionStr == "include")
				newInfo->defaultAction.actn = action::INCLUDE;
			else if (actionStr == "exclude")
				newInfo->defaultAction.actn = action::EXCLUDE;
			else if (actionStr == "flatten")
				newInfo->defaultAction.actn = action::FLATTEN;
			else
			{
				Logger::getLogger()->error("Parse asset filter config, unable to parse defaultAction value: '%s'", filter->getConfig().getValue("config").c_str());
				return;
			}
			Logger::getLogger()->info("Parse asset filter config, default action for unmentioned asset names=%d", newInfo->defaultAction.actn);
		}
		
		Value &rules = document["rules"];
		if (!rules.IsArray())
		{
			Logger::getLogger()->error("Parse asset filter config, rules array is missing : '%s'", filter->getConfig().getValue("config").c_str());
			return;
		}
		for (Value::ConstValueIterator iter = rules.Begin(); iter != rules.End(); ++iter)
		{
			if (!iter->IsObject())
			{
				Logger::getLogger()->error("Parse asset filter config, each entry in rules array must be an object: '%s'", filter->getConfig().getValue("config").c_str());
				return;
			}
			if (!iter->HasMember("asset_name") || !iter->HasMember("action"))
			{
				Logger::getLogger()->error("Parse asset filter config, asset_name/action is not found in the entry: '%s'", filter->getConfig().getValue("config").c_str());
				return;
			}
			
			string asset_name = (*iter)["asset_name"].GetString();
			string actionStr= (*iter)["action"].GetString();
			string new_asset_name = "";
			map<string, string> dpmap;
			map<string, map<string,vector<string>>> splitAssets;
			string datapoint;
			string dpType;
			for (auto & c: actionStr) c = tolower(c);
			action actn;
			if (actionStr == "include")
				actn = action::INCLUDE;
			else if (actionStr == "exclude")
				actn = action::EXCLUDE;
			else if (actionStr == "rename")
			{
				actn = action::RENAME;
				if (iter->HasMember("new_asset_name"))
					new_asset_name = (*iter)["new_asset_name"].GetString();
				else
				{
					Logger::getLogger()->error("Parse asset filter config, new_asset_name is not found in the RENAME entry: '%s'", filter->getConfig().getValue("config").c_str());
					return;
				}
			}
			else if (actionStr == "datapointmap")
			{
				actn = action::DPMAP;
				if (iter->HasMember("map"))
				{
					const Value& map = (*iter)["map"];
					for (auto& mapit : map.GetObject())
						dpmap.insert(pair<string, string>(mapit.name.GetString(), mapit.value.GetString()));
				}
				else
				{
					Logger::getLogger()->error("Parse asset filter config, map is not found in the DatapointMap entry: '%s'", filter->getConfig().getValue("config").c_str());
					return;
				}
			}
			else if(actionStr == "remove")
			{
				actn = action::REMOVE;

				if (iter->HasMember("datapoint"))
                                        datapoint = (*iter)["datapoint"].GetString();

				if (iter->HasMember("type"))
                                        dpType = (*iter)["type"].GetString();

			}
			else if (actionStr == "flatten")
			{
				actn = action::FLATTEN;
			}
			else if (actionStr == "split")
			{
				actn = action::SPLIT;
				splitAssetConfigure(document["rules"], splitAssets);
			}
			Logger::getLogger()->info("Parse asset filter config, Adding to assetFilterConfig map: {%s, %d, %s}", asset_name.c_str(), actn, new_asset_name.c_str());
			(*newInfo->assetFilterConfig)[asset_name] = {actn, new_asset_name, dpmap, datapoint, dpType, splitAssets};
		}
		auto tmp = new std::map<std::string, AssetAction>;
		tmp = info->assetFilterConfig;
		info->assetFilterConfig = newInfo->assetFilterConfig;
		delete tmp;
		info->defaultAction = newInfo->defaultAction;
	}
}

/**
 * Call the shutdown method in the plugin
 */
void plugin_shutdown(PLUGIN_HANDLE *handle)
{
	FILTER_INFO *info = (FILTER_INFO *) handle;
	delete info->handle;
	delete info->assetFilterConfig;
	delete info;
}

// End of extern "C"
};

