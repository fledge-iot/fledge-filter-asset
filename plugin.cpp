/*
 * FogLAMP "asset" filter plugin.
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
        "1.0.0",                  // Version
        0,                        // Flags
        PLUGIN_TYPE_FILTER,       // Type
        "1.0.0",                  // Interface version
	DEFAULT_CONFIG	          // Default plugin configuration
};

typedef enum
{
	INCLUDE,
	EXCLUDE,
	RENAME 
} action;

struct AssetAction {
	action actn;
	string new_asset_name; // valid only in case of RENAME
};

typedef struct
{
	FogLampFilter *handle;
	std::map<std::string, AssetAction> *assetFilterConfig;
	AssetAction	defaultAction;
} FILTER_INFO;

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
	info->handle = new FogLampFilter(FILTER_NAME, *config, outHandle, output);
	FogLampFilter *filter = info->handle;
	
	// Handle filter configuration
	info->assetFilterConfig = new std::map<std::string, AssetAction>;
	if (filter->getConfig().itemExists("config"))
	{
		Document	document;
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
			else
			{
				Logger::getLogger()->error("Parse asset filter config, unable to parse defaultAction value: '%s'", filter->getConfig().getValue("config").c_str());
				return NULL;
			}
			Logger::getLogger()->info("Parse asset filter config, default action for unmentioned asset names=%d", info->defaultAction.actn);
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
			Logger::getLogger()->info("Parse asset filter config, Adding to assetFilterConfig map: {%s, %d, %s}", asset_name.c_str(), actn, new_asset_name.c_str());
			(*info->assetFilterConfig)[asset_name] = {actn, new_asset_name};
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
 * Ingest a set of readings into the plugin for processing
 *
 * @param handle	The plugin handle returned from plugin_init
 * @param readingSet	The readings to process
 */
void plugin_ingest(PLUGIN_HANDLE *handle,
		   READINGSET *readingSet)
{
	//Logger::getLogger()->info("AssetFilter: plugin_ingest()");
	FILTER_INFO *info = (FILTER_INFO *) handle;
	FogLampFilter* filter = info->handle;
	
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
			assetAction = &((*info->assetFilterConfig)[(*elem)->getAssetName()]);
			//Logger::getLogger()->info("Reading's asset_name=%s, matching map entry={%s, %d, %s}", 
				//	(*elem)->getAssetName().c_str(), (*elem)->getAssetName().c_str(), assetAction->actn, assetAction->new_asset_name.c_str());

		if(assetAction->actn == action::INCLUDE)
		{
			newReadings.push_back(new Reading(**elem)); // copy original Reading object
		}
		else if(assetAction->actn == action::EXCLUDE)
		{
			// no need to free memory allocated for original reading object: done in ReadingSet destructor
		}
		else if(assetAction->actn == action::RENAME)
		{
			 Reading *newRdng = new Reading(**elem); // copy original Reading object
			 newRdng->setAssetName(assetAction->new_asset_name);
			 newReadings.push_back(newRdng);
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
	FogLampFilter* filter = info->handle;

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
			Logger::getLogger()->info("Parse asset filter config, Adding to assetFilterConfig map: {%s, %d, %s}", asset_name.c_str(), actn, new_asset_name.c_str());
			(*newInfo->assetFilterConfig)[asset_name] = {actn, new_asset_name};
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

