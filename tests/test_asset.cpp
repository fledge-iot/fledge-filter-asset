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
static const char *dpmapTest = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"datapointmap\", \"map\" : { \"test\" : \"result\" } } ] }";

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

}

TEST(ASSET, rename)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", renameTest);
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
	plugin_ingest(handle, (READINGSET *)readingSet);


	vector<Reading *>results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 3);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "new");
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1000);

	out = results[1];
	ASSERT_STREQ(out->getAssetName().c_str(), "new");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "test");
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

}