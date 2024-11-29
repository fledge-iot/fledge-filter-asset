#include <gtest/gtest.h>
#include <plugin_api.h>
#include <config_category.h>
#include <filter_plugin.h>
#include <filter.h>
#include <string.h>
#include <string>
#include <rapidjson/document.h>
#include <reading.h>
#include <reading_set.h>

using namespace std;
using namespace rapidjson;

/*
 * Test that the plugin doesn't fail with bad configuration. Ideally we
 * should like to look at the erorrs raised but currently don't have
 * a mechanism for doing this. Therefore we are doigna basic sanity test.
 */

extern "C"
{
	PLUGIN_INFORMATION *plugin_info();
	void plugin_ingest(void *handle,
					   READINGSET *readingSet);
	PLUGIN_HANDLE plugin_init(ConfigCategory *config,
							  OUTPUT_HANDLE *outHandle,
							  OUTPUT_STREAM output);

	extern void Handler(void *handle, READINGSET *readings);
};

static const char *noRules = QUOTE({ });

static const char *badDefault = QUOTE({ "rules": [ { "asset_name": "system/cpuUsage_all", "action": "rename", "new_name": "foo" } ], "defaultAction" : "rubbish" });

static const char *badRule = QUOTE({ "rules": [ { "asset_name": "system/cpuUsage_all", "action": "rename", "new_name": "foo" }, "foo" ] });

static const char *badAction = QUOTE({ "rules": [ { "asset_name": "system/cpuUsage_all", "action": "not supported", "datapoint": "Z" } ] });

static const char *missingAction = QUOTE({ "rules": [ { "asset_name": "system/cpuUsage_all", "datapoint": "Z" } ] });

static const char *missingAsset = QUOTE({ "rules": [ { "action": "not supported", "datapoint": "Z" } ] });

static const char *renameNoNewName = QUOTE({ "rules": [ { "asset_name": "system/cpuUsage_all", "action": "rename", "datapoint": "Z" } ] });

static const char *selectAssets = QUOTE({ "rules": [ { "asset_name": "system/cpuUsage_all", "action": "select", "assets": [ "X", "Y" ] } ] });

static const char *dpmapNoMap = QUOTE({ "rules": [ { "asset_name": "system/cpuUsage_all", "action": "datapointmap", "datapoint": "Z" } ] });

static const char *removeNoOption = QUOTE({ "rules": [ { "asset_name": "system/cpuUsage_all", "action": "remove" } ] });

static const char *splitNotObject = QUOTE({ "rules": [ { "asset_name": "system/cpuUsage_all", "action": "split", "split" : "xxx"  } ] });

static const char *splitNotList = QUOTE({ "rules": [ { "asset_name": "system/cpuUsage_all", "action": "split", "split" : { "asset1" : "xxx" } } ] });

static const char *splitListType = QUOTE({ "rules": [ { "asset_name": "system/cpuUsage_all", "action": "split", "split" : { "asset1" : [ "xxx", 1 ] } } ] });


// Test the handling of a bad default action 
TEST(ASSET_BAD_CONFIG, BadDefaultAction)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", badDefault);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test the handling of a bad (non object) rule
TEST(ASSET_BAD_CONFIG, BadRule)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", badRule);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test the handling of a configuration without any rules
TEST(ASSET_BAD_CONFIG, NoRules)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", noRules);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test the handling of a configuration with a ruel that has an unsupported action
TEST(ASSET_BAD_CONFIG, BadAction)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", badAction);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test the handling of a configuration a rule with a missing action
TEST(ASSET_BAD_CONFIG, MissingAction)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", missingAction);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test the handling of a configuration without an asset in the rule
TEST(ASSET_BAD_CONFIG, MissingAsset)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", missingAsset);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test handlign of misonfigured rename action
TEST(ASSET_BAD_CONFIG, RenameNoNewName)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", renameNoNewName);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test handling of misconfigured select action
TEST(ASSET_BAD_CONFIG, SelectAssets)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", selectAssets);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test handling of misconfigured datapoiimntmap action
TEST(ASSET_BAD_CONFIG, DatapointMapMissing)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", dpmapNoMap);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test handling of misconfigured remove action
TEST(ASSET_BAD_CONFIG, RemoveMissing)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", removeNoOption);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test handling of misconfigured split action, the value of the split attribute is not an object
TEST(ASSET_BAD_CONFIG, SplitNotObject)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", splitNotObject);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test handling of misconfigured split action, the value of the split asset is not an list
TEST(ASSET_BAD_CONFIG, SplitNotList)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", splitNotList);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}

// Test handling of misconfigured split action, the value of the datapoitn naem is not a string
TEST(ASSET_BAD_CONFIG, SplitListType)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", splitListType);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);  
}
