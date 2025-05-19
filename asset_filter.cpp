/*
 * Fledge "asset" filter plugin.
 *
 * Copyright (c) 2025 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch           
 */
#include <asset_filter.h>

using namespace std;
using namespace rapidjson;

/**
 * Construct an asset filter. Call the base class constructor
 * and handle the configuration.
 */
AssetFilter::AssetFilter(const std::string& filterName,
				ConfigCategory& filterConfig,
				OUTPUT_HANDLE *outHandle,
				OUTPUT_STREAM out) :
					FledgeFilter(filterName, filterConfig, 
                                                outHandle, out)
{
	m_defaultRule = NULL;
	m_logger = Logger::getLogger();
	handleConfig(filterConfig);
	m_instanceName = filterConfig.getName();
}

/**
 * Destructor for the asset filter
 */
AssetFilter::~AssetFilter()
{
	// Remove any rules
	for (auto& r : m_rules)
		delete r;
	if (m_defaultRule)
		delete m_defaultRule;
}

/**
 * Handle the configuration of the asset filter
 *
 * @param category	The configuration category
 */
void AssetFilter::handleConfig(ConfigCategory& category)
{
	lock_guard<mutex> guard(m_configMutex);
	// Remove any rules
	for (auto& r : m_rules)
		delete r;
	m_rules.clear();

	if (category.itemExists("config"))
	{
		Document doc;
		string config = category.getValue("config");

		if (doc.Parse(config.c_str()).HasParseError())
		{
			m_logger->error("Unable to parse filter config: '%s'", config.c_str());
			return;
		}
		Value::MemberIterator defaultAction = doc.FindMember("defaultAction");
		if (defaultAction == doc.MemberEnd() || !defaultAction->value.IsString())
		{
			m_defaultRule = new IncludeRule(m_instanceName);
			m_logger->info("No default action found in the plugin rules");
		}
		else
		{
			string actionStr= defaultAction->value.GetString();
			for (auto & c: actionStr) c = tolower(c);
			if (actionStr == "include")
				m_defaultRule = new IncludeRule(m_instanceName);
			else if (actionStr == "exclude")
				m_defaultRule = new ExcludeRule(m_instanceName);
			else if (actionStr == "flatten")
				m_defaultRule = new FlattenRule(m_instanceName);
			else
				m_logger->error("The rule '%s' is not a valid default rule",
						actionStr.c_str());
		}
		if (!doc.HasMember("rules"))
		{
			if (m_defaultRule)
				m_logger->warn("The asset filter configuration is missing the rules item. The default rule %s rule will be applied to all assets.", m_defaultRule->getName().c_str());
			else
				m_logger->error("The asset filter configuration is missing the rules item. No action will be taken by the filter.");
			return;
		}
		Value &rules = doc["rules"];
		if (!rules.IsArray())
		{
			m_logger->error("The rules item in the asset filter configuration should be an array of rules objects. The filter will have no effect.");
			return;
		}
		for (Value::ConstValueIterator iter = rules.Begin(); iter != rules.End(); ++iter)
		{
			if (!iter->IsObject())
			{
				m_logger->error("Asset filter configuration parse error. Each entry in rules array must be an object. The filter will have no effect.");
				continue;
			}
			if (!iter->HasMember("asset_name"))
			{
				m_logger->error("The filter configuration contains a rule that has no asset_name property. This rule will be ignored.");
				continue;
			}
			if (!iter->HasMember("action"))
			{
				m_logger->error("The rule for asset '%s' has no 'action' property. This rule will be ignored.", (*iter)["asset_name"].GetString());
				continue;
			}
			
			string asset_name = (*iter)["asset_name"].GetString();
			string action = (*iter)["action"].GetString();
			if (action.compare("include") == 0)
				m_rules.emplace_back(new IncludeRule(m_instanceName, asset_name));
			else if (action.compare("exclude") == 0)
				m_rules.emplace_back(new ExcludeRule(m_instanceName, asset_name));
			else if (action.compare("rename") == 0)
				m_rules.emplace_back(new RenameRule(m_instanceName, asset_name, *iter));
			else if (action.compare("datapointmap") == 0)
				m_rules.emplace_back(new DatapointMapRule(m_instanceName, asset_name, *iter));
			else if (action.compare("remove") == 0)
				m_rules.emplace_back(new RemoveRule(m_instanceName, asset_name, *iter));
			else if (action.compare("flatten") == 0)
				m_rules.emplace_back(new FlattenRule(m_instanceName, asset_name));
			else if (action.compare("split") == 0)
				m_rules.emplace_back(new SplitRule(m_instanceName, asset_name, *iter));
			else if (action.compare("select") == 0)
				m_rules.emplace_back(new SelectRule(m_instanceName, asset_name, *iter));
			else if (action.compare("retain") == 0)
				m_rules.emplace_back(new SelectRule(m_instanceName, asset_name, *iter));
			else if (action.compare("nest") == 0)
				m_rules.emplace_back(new NestRule(m_instanceName, asset_name, *iter));
			else
				m_logger->error("Unrecognised action '%s'", action.c_str());
		}
	}
}

/**
 * Process the readings by executing all the rules in turn
 * that match the asset name in each reading.
 *
 * NB Each input reading may result in zero or more output readings
 *
 * @param input	The readings to be processed
 * @param out	The resultant readings
 */
void AssetFilter::ingest(READINGSET *input, vector<Reading*>& out)
{
	vector<Reading *> readings = input->getAllReadings();

	for (Reading *reading : readings)
	{
		if (m_rules.size() == 0)
		{
			// We have no rules, run the default rule if there
			// is one otherwise copy the reading through
			if (m_defaultRule)
				m_defaultRule->execute(reading, out);
			else
				out.emplace_back(reading);
		}
		else
		{
			int matches = 0; 
			vector<Rule *>::const_iterator rule = m_rules.begin();
			if (rule != m_rules.end())
			{
				matches = processReading(reading, out, rule, matches);
				if (matches == 0 && m_defaultRule)
				{
					// No rules matched so run the default rule
					m_defaultRule->execute(reading, out);
				}
				else if (matches == 0)
				{
					// No rules matched and we have no default rule
					out.emplace_back(reading);
				}
			}
		}
	}

	// The readings in the input reading set will either have
	// been deleted or reused by the rules. Therefore we must clear
	// the reading set before deleting it to avoid double calls to
	// delete for the readings.
	input->clear();
	delete input;
}

/**
 * Recursively process all the rules against a reading.
 * Each rule may result in zero of more readings resulting
 * from the execution of the reading. Therefore each rule
 * produces a vector of readings. We then move to the next
 * rule and execute that on each of the readings in the
 * result form the previous rule. Once we run out of new
 * rules to execute we add the resultant vector of readings
 * produced by the final rule in the list to the output vector.
 *
 * If a rule produces no results then we end the recursive
 * execution of the rules at that point.
 *
 * @param reading	The reading to process
 * @param out		The final output vector to add the results of all rule executions to
 * @param rule		Iterator on the rules to process
 */
int AssetFilter::processReading(Reading *reading, vector<Reading *>& out, vector<Rule *>::const_iterator rule, int matches)
{
vector<Reading *> result;

	// Execute the rule on the reading if the reading matches
	// the asset name or pattern in the rule. Otherwise simply
	// execute the next rule on this reading.
	if ((*rule)->match(reading))
	{
		(*rule)->execute(reading, result);
		matches++;
	}
	else
	{
		result.emplace_back(reading);
	}
	if (result.empty())
	{
		// The rule has removed the reading, therefore
		// we need not process any more rules
		return matches;
	}
	rule++;
	if (rule != m_rules.end())
	{
		for (Reading *nReading : result)
		{
			matches = processReading(nReading, out, rule, matches);
		}
	}
	else if (matches > 0)
	{
		// If any rules matched put the resulting readings in the 
		// final output buffer. Otherwise it will be picked up by
		// the defaultRule at the end and processed.
		for (Reading *nReading : result)
		{
			out.emplace_back(nReading);
		}
	}
	return matches;
}

/**
 * Reconfigure the filter
 *
 * @param config	The new configuration
 */
void AssetFilter::reconfigure(const string& config)
{
	setConfig(config);
	ConfigCategory conf("AssetFilter", config);
	handleConfig(conf);
}
