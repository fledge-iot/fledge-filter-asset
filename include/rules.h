#ifndef _RULES_H
#define _RULES_H
/*
 * Fledge "asset" filter plugin rules classes.
 *
 * Copyright (c) 2025 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <logger.h>
#include <reading.h>
#include <map>
#include <regex>

/**
 * The base rule class upon which all rules are implemented.
 *
 * The base class implements some common functionality, including
 * the name matching mechanism for the rules. Matches may be
 * either based on the exact match of a rule or by using a regex.
 * Since regex is comparitively slow we cache the compiled regex
 * expression and only use regex if the asset name in the rule
 * contains any special characters.
 */
class Rule {
	public:
		Rule(const std::string& asset);
		virtual ~Rule();
		virtual void	execute(Reading *reading, std::vector<Reading *>& out) = 0;
		bool		match(Reading *reading);
		std::string	getName() { return m_asset; };
	protected:
		bool		isRegexString(const std::string& str);
	protected:
		Logger		*m_logger;
		std::string	m_asset;
		bool		m_assetIsRegex;
		std::regex	*m_asset_re;
};

/**
 * The include rule, any matching reading is included
 *
 * This may be a default action so has a constructor that does not
 * require an asset name.
 */
class IncludeRule : public Rule {
	public:
		IncludeRule(const std::string& asset);
		IncludeRule();
		~IncludeRule();
		void		execute(Reading *reading, std::vector<Reading *>& out);
};

/**
 * The exclude rule, any matching reading is excluded
 *
 * This may be a default action so has a constructor that does not
 * require an asset name.
 */
class ExcludeRule : public Rule {
	public:
		ExcludeRule(const std::string& asset);
		ExcludeRule();
		~ExcludeRule();
		void		execute(Reading *reading, std::vector<Reading *>& out);
};

/**
 * The rename rule. Any matching rule is rename. Regular expressions can be
 * used to rename multiple assets in a single rule.
 */
class RenameRule : public Rule {
	public:
		RenameRule(const std::string& asset, const rapidjson::Value& json);
		~RenameRule();
		void		execute(Reading *reading, std::vector<Reading *>& out);
	private:
		std::string	m_newName;
		bool		m_isRegex;
		std::regex	*m_newRegex;
};

/**
 * The remove rule. Remove one or more datapoints from a matching reading.
 */
class RemoveRule : public Rule {
	public:
		RemoveRule(const std::string& asset, const rapidjson::Value& json);
		~RemoveRule();
		void		execute(Reading *reading, std::vector<Reading *>& out);
	private:
		bool		validateType(const std::string& type);
	private:
		std::string	m_datapoint;
		std::regex	*m_regex;
		std::string	m_type;
		std::vector<std::string>
				m_datapoints;
};

/**
 * The flatten rule. Flatten datapoints in any matching readings.
 *
 * This may be a default action so has a constructor that does not
 * require an asset name.
 */
class FlattenRule : public Rule {
	public:
		FlattenRule(const std::string& asset);
		FlattenRule();
		~FlattenRule();
		void		execute(Reading *reading, std::vector<Reading *>& out);
	private:
		void		flattenDatapoint(Datapoint *datapoint,  std::string datapointName, std::vector<Datapoint *>& flattenDatapoints);
};

/**
 * Datapoint map rule. Map the names of datapoints in matching readings.
 */
class DatapointMapRule : public Rule {
	public:
		DatapointMapRule(const std::string& asset, const rapidjson::Value& json);
		~DatapointMapRule();
		void         execute(Reading *reading, std::vector<Reading *>& out);
	private:
		std::map<std::string, std::string> m_dpMap;
		std::map<std::regex *, std::string>  m_dpRegexMap;
};

/**
 * The split rule. Split a reading with multiple datapoints into two or more readings
 */
class SplitRule : public Rule {
	public:
		SplitRule(const std::string& asset, const rapidjson::Value& json);
		~SplitRule();
		void         execute(Reading *reading, std::vector<Reading *>& out);
		void	     setName(const std::string& name) { m_pluginName = name; };
	private:
		std::map<std::string, std::vector<std::string>> m_split;
		std::string		m_pluginName;
};

/**
 * Select Rule. Select a set of datapoints to include in the reading.
 */
class SelectRule : public Rule {
	public:
		SelectRule(const std::string& asset, const rapidjson::Value& json);
		~SelectRule();
		void         execute(Reading *reading, std::vector<Reading *>& out);
	private:
		bool	     validateType(const std::string& type);
	private:
		std::vector<std::string>
				m_datapoints;
		std::vector<std::regex>
				m_regexes;
		std::string	m_type;
};

/**
 * The nest rule. Nest a reading with multiple datapoints into tree structured datapoints
 */
class NestRule : public Rule {
	public:
		NestRule(const std::string& asset, const rapidjson::Value& json);
		~NestRule();
		void         execute(Reading *reading, std::vector<Reading *>& out);
	private:
		std::map<std::string, std::vector<std::string>> m_nest;
};
#endif
