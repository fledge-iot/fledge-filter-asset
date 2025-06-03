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

extern "C" {
	PLUGIN_INFORMATION *plugin_info();
	void plugin_ingest(void *handle,
                   READINGSET *readingSet);
	PLUGIN_HANDLE plugin_init(ConfigCategory* config,
			  OUTPUT_HANDLE *outHandle,
			  OUTPUT_STREAM output);
	void plugin_shutdown(PLUGIN_HANDLE handle);
	int called = 0;

	void Handler(void *handle, READINGSET *readings)
	{
		called++;
		*(READINGSET **)handle = readings;
	}
};

static const char *excludeTest = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"exclude\" } ] }";
static const char *includeTest = "{ \"rules\" : [ { \"asset_name\" : \"test1\", \"action\" : \"include\" } ], \"defaultAction\" : \"exclude\" }";
static const char *renameTest = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"rename\", \"new_asset_name\" : \"new\" } ] }";
static const char *renameRegexTest = "{ \"rules\" : [ { \"asset_name\" : \"test([0-9]*)\", \"action\" : \"rename\", \"new_asset_name\" : \"new$1\" } ] }";
static const char *dpmapTest = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"datapointmap\", \"map\" : { \"test\" : \"result\" } } ] }";
static const char *dpmapTestRegex = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"datapointmap\", \"map\" : { \"test(.*)\" : \"result$1\" } } ] }";

static const char *removeTest_1 = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"remove\", \"datapoint\" : \"test_r\" } ] }";
static const char *removeTest_2 = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"remove\", \"type\" : \"INTEGER\" } ] }";

static const char *removeTest_3 = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"remove\", \"type\" : \"number\" } ] }";

static const char *removeTest_4 = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"remove\", \"type\" : \"NON-NUMERIC\" } ] }";

static const char *flattenDatapointTest = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"flatten\" } ] }";

static const char *action_split_with_key = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"split\" , \"split\": { \"test_1\": [\"Floor1\", \"Floor2\"], \"test_2\": [\"Floor1\"] } } ] }";

static const char *action_split_without_key = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"split\" } ] }";

static const char *action_split_regex_with_key = "{ \"rules\" : [ { \"asset_name\" : \"test([0-9]*)\", \"action\" : \"split\" , \"split\": { \"test_1_$1\": [\"Floor1\", \"Floor2\"], \"test_2_$1\": [\"Floor1\"] } } ] }";

static const char *action_split_with_extra_DP = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"split\" , \"split\": { \"test_1\": [\"Floor1\", \"Floor2\"] } } ] }";

static const char *action_select = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"select\" , \"datapoints\": [ \"voltage\", \"current\"] } ] }";
static const char *action_select_regex = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"select\" , \"datapoints\": [ \"volt.*\", \"current\"] } ] }";
static const char *action_select_single = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"select\" , \"datapoint\": \"current\" } ] }";
static const char *action_select_single_regex = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"select\" , \"datapoint\": \"voltage.*\" } ] }";
static const char *action_select_type = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"select\" , \"type\": \"float\" } ] }";
static const char *action_retain = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"retain\" , \"datapoints\": [ \"voltage\", \"current\"] } ] }";
static const char *action_nest = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"nest\" , \"nest\": { \"electrical\": [\"voltage\", \"current\", \"power\"], \"environmental\" : [ \"temperature\", \"humidity\" ] } } ] }";
static const char *renameRegexTooFewTest = "{ \"rules\" : [ { \"asset_name\" : \"test([0-9]*)\", \"action\" : \"rename\", \"new_asset_name\" : \"new$1$2\" } ] }";
static const char *renameRegexTooManyTest = "{ \"rules\" : [ { \"asset_name\" : \"test([0-9]*)([a-z]*)\", \"action\" : \"rename\", \"new_asset_name\" : \"new$1\" } ] }";


TEST(ASSET, exclude)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", excludeTest);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("test", dpv);
	readings->push_back(new Reading("test", value));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("test", dpv1);
	readings->push_back(new Reading("test", value1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("test", dpv2);
	readings->push_back(new Reading("test1", value2));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "test1");
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1140);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;

}

TEST(ASSET, include)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", includeTest);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("test", dpv);
	readings->push_back(new Reading("test", value));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("test", dpv1);
	readings->push_back(new Reading("test", value1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("test", dpv2);
	readings->push_back(new Reading("test1", value2));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);


	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "test1");
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1140);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

// Test renaming using a regex with too few bracketed expression
TEST(ASSET, renameRegexTooFew)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", renameRegexTooFewTest);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("test", dpv);
	readings->push_back(new Reading("test12", value));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("test", dpv1);
	readings->push_back(new Reading("test12", value1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("test", dpv2);
	readings->push_back(new Reading("test1", value2));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);


	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 3);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "new12");
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1000);

	out = results[1];
	ASSERT_STREQ(out->getAssetName().c_str(), "new12");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1001);

	out = results[2];
	ASSERT_STREQ(out->getAssetName().c_str(), "new1");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1140);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

// Test renaming using a regex with too many bracketed expression
TEST(ASSET, renameRegexTooMany)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", renameRegexTooManyTest);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("test", dpv);
	readings->push_back(new Reading("test12", value));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("test", dpv1);
	readings->push_back(new Reading("test12", value1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("test", dpv2);
	readings->push_back(new Reading("test1", value2));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);


	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 3);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "new12");
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1000);

	out = results[1];
	ASSERT_STREQ(out->getAssetName().c_str(), "new12");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1001);

	out = results[2];
	ASSERT_STREQ(out->getAssetName().c_str(), "new1");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1140);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

// Test renaming using a regex with bracketed a expression
TEST(ASSET, renameRegex)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", renameRegexTest);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("test", dpv);
	readings->push_back(new Reading("test12", value));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("test", dpv1);
	readings->push_back(new Reading("test12", value1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("test", dpv2);
	readings->push_back(new Reading("test1", value2));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);


	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 3);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "new12");
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1000);

	out = results[1];
	ASSERT_STREQ(out->getAssetName().c_str(), "new12");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1001);

	out = results[2];
	ASSERT_STREQ(out->getAssetName().c_str(), "new1");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1140);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, datapointmap)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", dpmapTest);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	long testValue = 1000;
	DatapointValue dpv(testValue);
	Datapoint *value = new Datapoint("test", dpv);
	readings->push_back(new Reading("test", value));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("test", dpv1);
	readings->push_back(new Reading("test", value1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("test", dpv2);
	readings->push_back(new Reading("test1", value2));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);


	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 3);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "test");
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "result");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1000);

	out = results[1];
	ASSERT_STREQ(out->getAssetName().c_str(), "test");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "result");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1001);

	out = results[2];
	ASSERT_STREQ(out->getAssetName().c_str(), "test1");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1140);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, datapointmap_regex)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", dpmapTestRegex);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	vector<Datapoint *> p1;
	long testValue = 1000;
	DatapointValue dpv(testValue);
	p1.emplace_back(new Datapoint("test0", dpv));
	long test1Value = 1010;
	DatapointValue dpvt1(test1Value);
	p1.emplace_back(new Datapoint("test1", dpvt1));
	readings->push_back(new Reading("test", p1));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("test1", dpv1);
	readings->push_back(new Reading("test", value1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("test1", dpv2);
	readings->push_back(new Reading("test1", value2));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);


	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 3);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "test");
	ASSERT_EQ(out->getDatapointCount(), 2);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 2);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "result0");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1000);
	outdp = points[1];
	ASSERT_STREQ(outdp->getName().c_str(), "result1");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1010);

	out = results[1];
	ASSERT_STREQ(out->getAssetName().c_str(), "test");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "result1");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1001);

	out = results[2];
	ASSERT_STREQ(out->getAssetName().c_str(), "test1");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test1");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1140);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, remove)
{
        PLUGIN_INFORMATION *info = plugin_info();
        ConfigCategory *config = new ConfigCategory("asset", info->config);
        ASSERT_NE(config, (ConfigCategory *)NULL);
        config->setItemsValueFromDefault();
        ASSERT_EQ(config->itemExists("config"), true);
        config->setValue("config", removeTest_1);
        config->setValue("enable", "true");
        ReadingSet *outReadings;
        void *handle = plugin_init(config, &outReadings, Handler);
        vector<Reading *> *readings = new vector<Reading *>;

        long testValue = 1000;
        DatapointValue dpv(testValue);
        Datapoint *value = new Datapoint("test", dpv);
        readings->push_back(new Reading("test", value));

        testValue = 1001;
        DatapointValue dpv1(testValue);
        Datapoint *value1 = new Datapoint("test", dpv1);
        readings->push_back(new Reading("test", value1));

        testValue = 1140;
        DatapointValue dpv2(testValue);
        Datapoint *value2 = new Datapoint("test_r", dpv2);
        readings->push_back(new Reading("test", value2));

        ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
        plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *>results = outReadings->getAllReadings();
        ASSERT_EQ(results.size(), 3);
        Reading *out = results[0];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 1);
        vector<Datapoint *> points = out->getReadingData();
        ASSERT_EQ(points.size(), 1);
        Datapoint *outdp = points[0];
        ASSERT_STREQ(outdp->getName().c_str(), "test");
        ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
        ASSERT_EQ(outdp->getData().toInt(), 1000);

        out = results[1];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 1);
        points = out->getReadingData();
        ASSERT_EQ(points.size(), 1);
        outdp = points[0];
        ASSERT_STREQ(outdp->getName().c_str(), "test");
        ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
        ASSERT_EQ(outdp->getData().toInt(), 1001);

	out = results[2];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 0);
        points = out->getReadingData();
        ASSERT_EQ(points.size(), 0);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}
TEST(ASSET, remove_2)
{
        PLUGIN_INFORMATION *info = plugin_info();
        ConfigCategory *config = new ConfigCategory("asset", info->config);
        ASSERT_NE(config, (ConfigCategory *)NULL);
        config->setItemsValueFromDefault();
        ASSERT_EQ(config->itemExists("config"), true);
        config->setValue("config", removeTest_2);
        config->setValue("enable", "true");
        ReadingSet *outReadings;
        void *handle = plugin_init(config, &outReadings, Handler);
        vector<Reading *> *readings = new vector<Reading *>;

        long testValue = 1000;
        DatapointValue dpv(testValue);
        Datapoint *value = new Datapoint("test", dpv);
        readings->push_back(new Reading("test", value));

	testValue = 1001;
        DatapointValue dpv1(testValue);
        Datapoint *value1 = new Datapoint("test", dpv1);
        readings->push_back(new Reading("test", value1));

        testValue = 1140;
        DatapointValue dpv2(testValue);
        Datapoint *value2 = new Datapoint("test_r", dpv2);
        readings->push_back(new Reading("test", value2));

        ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
        plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *>results = outReadings->getAllReadings();
        ASSERT_EQ(results.size(), 3);
        Reading *out = results[0];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 0);
        vector<Datapoint *> points = out->getReadingData();
        ASSERT_EQ(points.size(), 0);

        out = results[1];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 0);
        points = out->getReadingData();
        ASSERT_EQ(points.size(), 0);

        out = results[2];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 0);
        points = out->getReadingData();
        ASSERT_EQ(points.size(), 0);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, remove_3)
{
        PLUGIN_INFORMATION *info = plugin_info();
        ConfigCategory *config = new ConfigCategory("asset", info->config);
        ASSERT_NE(config, (ConfigCategory *)NULL);
        config->setItemsValueFromDefault();
        ASSERT_EQ(config->itemExists("config"), true);
        config->setValue("config", removeTest_3);
        config->setValue("enable", "true");
        ReadingSet *outReadings;
        void *handle = plugin_init(config, &outReadings, Handler);
        vector<Reading *> *readings = new vector<Reading *>;

        long testValue = 1000;
        DatapointValue dpv(testValue);
        Datapoint *value = new Datapoint("test", dpv);
        readings->push_back(new Reading("test", value));

	testValue = 1001;
        DatapointValue dpv1(testValue);
        Datapoint *value1 = new Datapoint("test", dpv1);
        readings->push_back(new Reading("test", value1));

        testValue = 1140;
        DatapointValue dpv2(testValue);
        Datapoint *value2 = new Datapoint("test_r", dpv2);
        readings->push_back(new Reading("test", value2));

        ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
        plugin_ingest(handle, (READINGSET *)readingSet);

        vector<Reading *>results = outReadings->getAllReadings();
        ASSERT_EQ(results.size(), 3);
        Reading *out = results[0];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 0);
        vector<Datapoint *> points = out->getReadingData();
        ASSERT_EQ(points.size(), 0);

        out = results[1];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 0);
        points = out->getReadingData();
        ASSERT_EQ(points.size(), 0);

        out = results[2];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 0);
        points = out->getReadingData();

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, remove_4)
{
        PLUGIN_INFORMATION *info = plugin_info();
        ConfigCategory *config = new ConfigCategory("asset", info->config);
        ASSERT_NE(config, (ConfigCategory *)NULL);
        config->setItemsValueFromDefault();
        ASSERT_EQ(config->itemExists("config"), true);
        config->setValue("config", removeTest_4);
        config->setValue("enable", "true");
        ReadingSet *outReadings;
        void *handle = plugin_init(config, &outReadings, Handler);
        vector<Reading *> *readings = new vector<Reading *>;

        long testValue = 1000;
        DatapointValue dpv(testValue);
        Datapoint *value = new Datapoint("test", dpv);
        readings->push_back(new Reading("test", value));

        testValue = 1001;
	DatapointValue dpv1(testValue);
        Datapoint *value1 = new Datapoint("test", dpv1);
        readings->push_back(new Reading("test", value1));

        testValue = 1140;
        DatapointValue dpv2(testValue);
        Datapoint *value2 = new Datapoint("test_r", dpv2);
        readings->push_back(new Reading("test", value2));

        ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
        plugin_ingest(handle, (READINGSET *)readingSet);

        vector<Reading *>results = outReadings->getAllReadings();
        ASSERT_EQ(results.size(), 3);

        Reading *out = results[0];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 1);
        vector<Datapoint *> points = out->getReadingData();
        ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
        ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
        ASSERT_EQ(outdp->getData().toInt(), 1000);

        out = results[1];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 1);
        points = out->getReadingData();
        ASSERT_EQ(points.size(), 1);
	outdp = points[0];
        ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
        ASSERT_EQ(outdp->getData().toInt(), 1001);

        out = results[2];
        ASSERT_STREQ(out->getAssetName().c_str(), "test");
        ASSERT_EQ(out->getDatapointCount(), 1);
        points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
        ASSERT_STREQ(outdp->getName().c_str(), "test_r");
 	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
        ASSERT_EQ(outdp->getData().toInt(), 1140);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, flattenDatapointWithoutNesting)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", flattenDatapointTest);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;
	const char *NestedReadingJSON = R"(
	{
		"pressure": 25
	})";

	readings->push_back(new Reading("test", NestedReadingJSON));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	ASSERT_EQ(results[0]->getAssetName(), "test");
	ASSERT_EQ(results[0]->getDatapoint("pressure")->getName(), "pressure");

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, flattenDatapointDic)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", flattenDatapointTest);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;
	const char *NestedReadingJSON = R"(
	{
		"pressure": {"floor1":30, "floor2":34, "floor3":36 }
	})";

	readings->push_back(new Reading("test", NestedReadingJSON));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	ASSERT_EQ(results[0]->getAssetName(), "test");

	ASSERT_EQ(results[0]->getDatapoint("pressure_floor1")->getName(), "pressure_floor1");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor2")->getName(), "pressure_floor2");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor3")->getName(), "pressure_floor3");

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, flattenDatapointDictInsideDic)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", flattenDatapointTest);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;
	const char *NestedReadingJSON = R"(
	{
		"pressure": {"floor1":30, "floor2":34, "floor3":{ "room1":38 , "room2":60 , "room3":40 } }
	})";

	readings->push_back(new Reading("test", NestedReadingJSON));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	ASSERT_EQ(results[0]->getAssetName(), "test");

	ASSERT_EQ(results[0]->getDatapoint("pressure_floor1")->getName(), "pressure_floor1");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor2")->getName(), "pressure_floor2");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor3_room1")->getName(), "pressure_floor3_room1");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor3_room2")->getName(), "pressure_floor3_room2");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor3_room3")->getName(), "pressure_floor3_room3");

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, assetsplit_1)
{
	// Test split action with split key
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_split_with_key);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long floor1 = 30;
	long floor2 = 34;
	DatapointValue dpv1(floor1);
	DatapointValue dpv2(floor2);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("Floor1", dpv1));
	dataPoints.push_back(new Datapoint("Floor2", dpv2));
	readings.push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 2);
	// test_1 asset have two data points Floor1 & Floor2
	ASSERT_EQ(results[0]->getAssetName(), "test_1");
	ASSERT_EQ(results[0]->getDatapoint("Floor1")->getName(), "Floor1");
	ASSERT_EQ(results[0]->getDatapoint("Floor2")->getName(), "Floor2");
	ASSERT_EQ(results[0]->getDatapoint("Floor1")->getData().toInt(), 30);
	ASSERT_EQ(results[0]->getDatapoint("Floor2")->getData().toInt(), 34);

	// test_2 asset have one data point Floor2
	ASSERT_EQ(results[1]->getAssetName(), "test_2");
	ASSERT_EQ(results[1]->getDatapoint("Floor1")->getName(), "Floor1");
	ASSERT_EQ(results[1]->getDatapoint("Floor1")->getData().toInt(), 30);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, assetsplit_2)
{
	// Test split action without split key
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_split_without_key);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long floor1 = 30;
	long floor2 = 34;
	DatapointValue dpv1(floor1);
	DatapointValue dpv2(floor2);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("Floor1", dpv1));
	dataPoints.push_back(new Datapoint("Floor2", dpv2));
	readings.push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 2);
	// test_1 asset have two data points Floor1 & Floor2
	ASSERT_EQ(results[0]->getAssetName(), "test_Floor1");
	ASSERT_EQ(results[0]->getDatapoint("Floor1")->getName(), "Floor1");
	ASSERT_EQ(results[0]->getDatapoint("Floor1")->getData().toInt(), 30);

	// test_2 asset have one data point Floor2
	ASSERT_EQ(results[1]->getAssetName(), "test_Floor2");
	ASSERT_EQ(results[1]->getDatapoint("Floor2")->getName(), "Floor2");
	ASSERT_EQ(results[1]->getDatapoint("Floor2")->getData().toInt(), 34);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, assetsplit_3)
{
	// Test split action with split key having datapoint for split asset which doesn't exists into asset
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_split_with_extra_DP);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long floor1 = 30;

	DatapointValue dpv1(floor1);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("Floor1", dpv1));
	readings.push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	// test_1 asset have two data points Floor1
	ASSERT_EQ(results[0]->getAssetName(), "test_1");
	ASSERT_EQ(results[0]->getDatapoint("Floor1")->getName(), "Floor1");
	ASSERT_EQ(results[0]->getDatapoint("Floor1")->getData().toInt(), 30);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, assetsplit_regex)
{
	// Test split action with split key
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_split_regex_with_key);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long floor1 = 30;
	long floor2 = 34;
	DatapointValue dpv1(floor1);
	DatapointValue dpv2(floor2);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("Floor1", dpv1));
	dataPoints.push_back(new Datapoint("Floor2", dpv2));
	readings.push_back(new Reading("test42", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 2);
	// test_1 asset have two data points Floor1 & Floor2
	ASSERT_EQ(results[0]->getAssetName(), "test_1_42");
	ASSERT_EQ(results[0]->getDatapoint("Floor1")->getName(), "Floor1");
	ASSERT_EQ(results[0]->getDatapoint("Floor2")->getName(), "Floor2");
	ASSERT_EQ(results[0]->getDatapoint("Floor1")->getData().toInt(), 30);
	ASSERT_EQ(results[0]->getDatapoint("Floor2")->getData().toInt(), 34);

	// test_2 asset have one data point Floor2
	ASSERT_EQ(results[1]->getAssetName(), "test_2_42");
	ASSERT_EQ(results[1]->getDatapoint("Floor1")->getName(), "Floor1");
	ASSERT_EQ(results[1]->getDatapoint("Floor1")->getData().toInt(), 30);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, select)
{
	// Test the select action
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_select);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long voltage = 30;
	double current = 0.120;
	double power = voltage * current;

	DatapointValue dpv1(voltage);
	DatapointValue dpv2(current);
	DatapointValue dpv3(power);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("voltage", dpv1));
	dataPoints.push_back(new Datapoint("current", dpv2));
	dataPoints.push_back(new Datapoint("power", dpv3));
	readings.push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	// test asset should have two data points voltage and current
	ASSERT_EQ(results[0]->getDatapointCount(), 2);
	ASSERT_EQ(results[0]->getAssetName(), "test");
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getName(), "voltage");
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getData().toInt(), 30);
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getData().getTypeStr(), "INTEGER");
	ASSERT_EQ(results[0]->getDatapoint("current")->getName(), "current");
	ASSERT_EQ(results[0]->getDatapoint("current")->getData().toDouble(), 0.12);
	ASSERT_EQ(results[0]->getDatapoint("current")->getData().getTypeStr(), "FLOAT");

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, select_missing)
{
	// Test the select action when not all data points are present
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_select);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long voltage = 30;
	double wattage = 450;
	double power = voltage / wattage;

	DatapointValue dpv1(voltage);
	DatapointValue dpv2(wattage);
	DatapointValue dpv3(power);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("voltage", dpv1));
	dataPoints.push_back(new Datapoint("wattage", dpv2));
	dataPoints.push_back(new Datapoint("power", dpv3));
	readings.push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	// test asset should have one data point voltage
	ASSERT_EQ(results[0]->getDatapointCount(), 1);
	ASSERT_EQ(results[0]->getAssetName(), "test");
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getName(), "voltage");
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getData().toInt(), 30);
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getData().getTypeStr(), "INTEGER");

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, select_regex)
{
	// Test the select action
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_select_regex);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long voltage = 30;
	double current = 0.120;
	double power = voltage * current;

	DatapointValue dpv1(voltage);
	DatapointValue dpv2(current);
	DatapointValue dpv3(power);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("voltage", dpv1));
	dataPoints.push_back(new Datapoint("current", dpv2));
	dataPoints.push_back(new Datapoint("power", dpv3));
	readings.push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	// test asset should have two data points voltage and current
	ASSERT_EQ(results[0]->getDatapointCount(), 2);
	ASSERT_EQ(results[0]->getAssetName(), "test");
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getName(), "voltage");
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getData().toInt(), 30);
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getData().getTypeStr(), "INTEGER");
	ASSERT_EQ(results[0]->getDatapoint("current")->getName(), "current");
	ASSERT_EQ(results[0]->getDatapoint("current")->getData().toDouble(), 0.12);
	ASSERT_EQ(results[0]->getDatapoint("current")->getData().getTypeStr(), "FLOAT");

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, select_single)
{
	// Test the select action
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_select_single);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long voltage = 30;
	double current = 0.120;
	double power = voltage * current;

	DatapointValue dpv1(voltage);
	DatapointValue dpv2(current);
	DatapointValue dpv3(power);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("voltage", dpv1));
	dataPoints.push_back(new Datapoint("current", dpv2));
	dataPoints.push_back(new Datapoint("power", dpv3));
	readings.push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	// test asset should have two data points voltage and current
	ASSERT_EQ(results[0]->getDatapointCount(), 1);
	ASSERT_EQ(results[0]->getAssetName(), "test");
	ASSERT_EQ(results[0]->getDatapoint("current")->getName(), "current");
	ASSERT_EQ(results[0]->getDatapoint("current")->getData().toDouble(), 0.12);
	ASSERT_EQ(results[0]->getDatapoint("current")->getData().getTypeStr(), "FLOAT");

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}


TEST(ASSET, select_single_regex)
{
	// Test the select action
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_select_single_regex);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long voltage = 30;
	double current = 0.120;
	double voltage2 = 28.7;

	DatapointValue dpv1(voltage);
	DatapointValue dpv2(current);
	DatapointValue dpv3(voltage2);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("voltage", dpv1));
	dataPoints.push_back(new Datapoint("current", dpv2));
	dataPoints.push_back(new Datapoint("voltage2", dpv3));
	readings.push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	// test asset should have two data points voltage and current
	ASSERT_EQ(results[0]->getDatapointCount(), 2);
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getName(), "voltage");
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getData().toInt(), 30);
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getData().getTypeStr(), "INTEGER");
	ASSERT_EQ(results[0]->getDatapoint("voltage2")->getName(), "voltage2");
	ASSERT_EQ(results[0]->getDatapoint("voltage2")->getData().toDouble(), 28.7);
	ASSERT_EQ(results[0]->getDatapoint("voltage2")->getData().getTypeStr(), "FLOAT");
	ASSERT_EQ(results[0]->getAssetName(), "test");

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, select_type)
{
	// Test the select action
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_select_type);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long voltage = 30;
	double current = 0.120;
	double power = voltage * current;

	DatapointValue dpv1(voltage);
	DatapointValue dpv2(current);
	DatapointValue dpv3(power);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("voltage", dpv1));
	dataPoints.push_back(new Datapoint("current", dpv2));
	dataPoints.push_back(new Datapoint("power", dpv3));
	readings.push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	// test asset should have two data points voltage and current
	ASSERT_EQ(results[0]->getDatapointCount(), 2);
	ASSERT_EQ(results[0]->getAssetName(), "test");
	ASSERT_EQ(results[0]->getDatapoint("current")->getName(), "current");
	ASSERT_EQ(results[0]->getDatapoint("current")->getData().toDouble(), 0.12);
	ASSERT_EQ(results[0]->getDatapoint("current")->getData().getTypeStr(), "FLOAT");
	ASSERT_EQ(results[0]->getDatapoint("power")->getName(), "power");
	ASSERT_EQ(results[0]->getDatapoint("power")->getData().toDouble(), voltage * current);
	ASSERT_EQ(results[0]->getDatapoint("power")->getData().getTypeStr(), "FLOAT");

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

TEST(ASSET, retain)
{
	// Test the select action
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_retain);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long voltage = 30;
	double current = 0.120;
	double power = voltage * current;

	DatapointValue dpv1(voltage);
	DatapointValue dpv2(current);
	DatapointValue dpv3(power);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("voltage", dpv1));
	dataPoints.push_back(new Datapoint("current", dpv2));
	dataPoints.push_back(new Datapoint("power", dpv3));
	readings.push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	// test asset should have two data points voltage and current
	ASSERT_EQ(results[0]->getDatapointCount(), 2);
	ASSERT_EQ(results[0]->getAssetName(), "test");
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getName(), "voltage");
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getData().toInt(), 30);
	ASSERT_EQ(results[0]->getDatapoint("voltage")->getData().getTypeStr(), "INTEGER");
	ASSERT_EQ(results[0]->getDatapoint("current")->getName(), "current");
	ASSERT_EQ(results[0]->getDatapoint("current")->getData().toDouble(), 0.12);
	ASSERT_EQ(results[0]->getDatapoint("current")->getData().getTypeStr(), "FLOAT");

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}


TEST(ASSET, nest)
{
	// Test the select action
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", action_nest);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> readings;
	long voltage = 30;
	double current = 0.120;
	double power = voltage * current;
	double temperature = 24.3;
	long humidity = 46;

	DatapointValue dpv1(voltage);
	DatapointValue dpv2(current);
	DatapointValue dpv3(power);
	DatapointValue dpv4(temperature);
	DatapointValue dpv5(humidity);
	DatapointValue dpv6("Running");
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("voltage", dpv1));
	dataPoints.push_back(new Datapoint("current", dpv2));
	dataPoints.push_back(new Datapoint("power", dpv3));
	dataPoints.push_back(new Datapoint("temperature", dpv4));
	dataPoints.push_back(new Datapoint("humidity", dpv5));
	dataPoints.push_back(new Datapoint("status", dpv6));
	readings.push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(&readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	// test asset should have two data points voltage and current
	ASSERT_EQ(results[0]->getDatapointCount(), 3);
	ASSERT_EQ(results[0]->getAssetName(), "test");
	ASSERT_EQ(results[0]->getDatapoint("electrical")->getName(), "electrical");
	ASSERT_EQ(results[0]->getDatapoint("electrical")->getData().getTypeStr(), "DP_DICT");
	ASSERT_EQ(results[0]->getDatapoint("environmental")->getName(), "environmental");
	ASSERT_EQ(results[0]->getDatapoint("environmental")->getData().getTypeStr(), "DP_DICT");
	ASSERT_EQ(results[0]->getDatapoint("status")->getName(), "status");
	ASSERT_EQ(results[0]->getDatapoint("status")->getData().getTypeStr(), "STRING");

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}
