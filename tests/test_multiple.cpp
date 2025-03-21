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

static const char *remove_multiple_dps = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"remove\", \"datapoint\" : \"DP1\" }, { \"asset_name\" : \"test\", \"action\" : \"remove\", \"datapoint\" : \"DP3\" } ] }";
static const char *rename_after_exclude = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"exclude\" }, { \"asset_name\" : \"test\", \"action\" : \"rename\", \"new_asset_name\" : \"NewAsset\" } ] }";
static const char *exclude_splitted_asset = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"split\" , \"split\": { \"test_1\": [\"DP1\", \"DP2\"], \"test_2\": [\"DP3\", \"DP4\"] } }, { \"asset_name\" : \"test_1\", \"action\" : \"exclude\" } ] }";
static const char *rename_asset_rename_dp_using_old_asset = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"rename\", \"new_asset_name\" : \"NewAsset\" }, { \"asset_name\" : \"test\", \"action\" : \"datapointmap\", \"map\" : { \"test\" : \"NewDP\" } } ] }";
static const char *rename_asset_rename_dp_using_new_asset = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"rename\", \"new_asset_name\" : \"NewAsset\" }, { \"asset_name\" : \"NewAsset\", \"action\" : \"datapointmap\", \"map\" : { \"test\" : \"NewDP\" } } ] }";


// Test Multiple Rules on same asset - Delete Multiple datapoints from same asset
TEST(ASSET_MULTIPLE_RULE, RemoveTwoDPs)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", remove_multiple_dps);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	std::vector<Datapoint *> dpVec;
	dpVec.emplace_back(new Datapoint("DP1", dpv));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	dpVec.emplace_back(new Datapoint("DP2", dpv1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	dpVec.emplace_back(new Datapoint("DP3", dpv2));
	readings->push_back(new Reading("test", dpVec));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "test");
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "DP2");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1001);
}

// Test Multiple Rules on same asset - Rename an asset after excluding it
TEST(ASSET_MULTIPLE_RULE, RenameAfterExclude)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", rename_after_exclude);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("test", dpv);
	readings->push_back(new Reading("test", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 0); // No further action can be performed on the excluded PR
}

// Test Multiple Rules on same asset - Exclude one of the splitted asset
TEST(ASSET_MULTIPLE_RULE, ExcludeSplittedAsset)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", exclude_splitted_asset);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	std::vector<Datapoint *> dpVec;
	dpVec.emplace_back(new Datapoint("DP1", dpv));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	dpVec.emplace_back(new Datapoint("DP2", dpv1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	dpVec.emplace_back(new Datapoint("DP3", dpv2));

	testValue = 1500;
	DatapointValue dpv3(testValue);
	dpVec.emplace_back(new Datapoint("DP4", dpv3));
	readings->push_back(new Reading("test", dpVec));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1); // Splitted Asset test_1 has been
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "test_2");
	ASSERT_EQ(out->getDatapointCount(), 2);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 2);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "DP3");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1140);

	outdp = points[1];
	ASSERT_STREQ(outdp->getName().c_str(), "DP4");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1500);
}

// Test Multiple Rules on same asset - 1. Rename an asset, 2. Rename datapoint using old asset name
TEST(ASSET_MULTIPLE_RULE, RenameDatapointUsingOldAsset)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", rename_asset_rename_dp_using_old_asset);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("test", dpv);
	readings->push_back(new Reading("test", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	ASSERT_STREQ(results[0]->getAssetName().c_str(), "NewAsset"); // Asset name changed from test to NewAsset
	ASSERT_STREQ((results[0]->getReadingData()[0])->getName().c_str(), "test"); // Datapoint name didn't change using old asset
}

// Test Multiple Rules on same asset - 1. Rename an asset, 2. Rename datapoint using new asset name
TEST(ASSET_MULTIPLE_RULE, RenameDatapointUsingNewAsset)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", rename_asset_rename_dp_using_new_asset);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("test", dpv);
	readings->push_back(new Reading("test", value));

	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	ASSERT_STREQ(results[0]->getAssetName().c_str(), "NewAsset"); // Asset name changed from test to NewAsset
	ASSERT_STREQ((results[0]->getReadingData()[0])->getName().c_str(), "NewDP"); // Datapoint name changed using new asset
}

