#ifndef _ASSET_FILTER_H
#define _ASSET_FILTER_H
/*
 * Fledge "asset" filter plugin.
 *
 * Copyright (c) 2025 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <string>
#include <plugin_api.h>
#include <config_category.h>
#include <filter.h>
#include <logger.h>
#include <reading_set.h>
#include <reading.h>
#include <rules.h>
#include <mutex>
#include <vector>

/**
 * The asset filter class. 
 *
 * Encapsulate the asset filter rules and provide the
 * entry points to execute those rules on the readings.
 *
 * All rules are processed in order on each reading. A rule
 * will only be run if the asset name in the reading matches
 * the asset name in the rule. Each rule may result in zero
 * or more readings beign returned for a single reading
 * passed into the rule.
 */
class AssetFilter : public FledgeFilter {
	public:
		AssetFilter(const std::string& filterName,
                        ConfigCategory& filterConfig,
                        OUTPUT_HANDLE *outHandle,
                        OUTPUT_STREAM out);
		~AssetFilter();
		void		ingest(READINGSET *input, std::vector<Reading*>& out);
		void		reconfigure(const std::string& conf);
	private:
		int		processReading(Reading *reading,
						std::vector<Reading *>& out,
						std::vector<Rule *>::const_iterator rule,
						int matches);
		void		handleConfig(ConfigCategory& category);
	private:
		Logger		*m_logger;
		std::mutex	m_configMutex;
		std::vector<Rule *>
				m_rules;
		Rule		*m_defaultRule;
};
#endif
