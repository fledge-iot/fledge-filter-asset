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

static const char *removeTest_1 = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"remove\", \"datapoint\" : \"test_r\" } ] }";
static const char *removeTest_2 = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"remove\", \"type\" : \"INTEGER\" } ] }";

static const char *removeTest_3 = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"remove\", \"type\" : \"number\" } ] }";

static const char *removeTest_4 = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"remove\", \"type\" : \"NON-NUMERIC\" } ] }";

static const char *flattenDatapointTest = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"flatten\" } ] }";

static const char *action_split_with_key = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"split\" , \"split\": { \"test_1\": [\"Floor1\", \"Floor2\"], \"test_2\": [\"Floor1\"] } } ] }";

static const char *action_split_without_key = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"split\" } ] }";

static const char *action_split_with_extra_DP = "{ \"rules\" : [ { \"asset_name\" : \"test\", \"action\" : \"split\" , \"split\": { \"test_1\": [\"Floor1\", \"Floor2\"] } } ] }";


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
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	ASSERT_EQ(results[0]->getAssetName(), "test");
	ASSERT_EQ(results[0]->getDatapoint("pressure")->getName(), "pressure");
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
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	ASSERT_EQ(results[0]->getAssetName(), "test");

	ASSERT_EQ(results[0]->getDatapoint("pressure_floor1")->getName(), "pressure_floor1");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor2")->getName(), "pressure_floor2");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor3")->getName(), "pressure_floor3");
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
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	ASSERT_EQ(results[0]->getAssetName(), "test");

	ASSERT_EQ(results[0]->getDatapoint("pressure_floor1")->getName(), "pressure_floor1");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor2")->getName(), "pressure_floor2");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor3_room1")->getName(), "pressure_floor3_room1");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor3_room2")->getName(), "pressure_floor3_room2");
	ASSERT_EQ(results[0]->getDatapoint("pressure_floor3_room3")->getName(), "pressure_floor3_room3");
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
	vector<Reading *> *readings = new vector<Reading *>;
	long floor1 = 30;
	long floor2 = 34;
	DatapointValue dpv1(floor1);
	DatapointValue dpv2(floor2);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("Floor1", dpv1));
	dataPoints.push_back(new Datapoint("Floor2", dpv2));
	readings->push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(readings);
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

	//Memory cleanup
	delete readings;
	delete readingSet;

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
	vector<Reading *> *readings = new vector<Reading *>;
	long floor1 = 30;
	long floor2 = 34;
	DatapointValue dpv1(floor1);
	DatapointValue dpv2(floor2);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("Floor1", dpv1));
	dataPoints.push_back(new Datapoint("Floor2", dpv2));
	readings->push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(readings);
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

	//Memory cleanup
	delete readings;
	delete readingSet;
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
	vector<Reading *> *readings = new vector<Reading *>;
	long floor1 = 30;

	DatapointValue dpv1(floor1);
	std::vector<Datapoint *> dataPoints;
	dataPoints.push_back(new Datapoint("Floor1", dpv1));
	readings->push_back(new Reading("test", dataPoints));
	ReadingSet *readingSet = new ReadingSet(readings);
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 1);
	// test_1 asset have two data points Floor1
	ASSERT_EQ(results[0]->getAssetName(), "test_1");
	ASSERT_EQ(results[0]->getDatapoint("Floor1")->getName(), "Floor1");
	ASSERT_EQ(results[0]->getDatapoint("Floor1")->getData().toInt(), 30);

	//Memory cleanup
	delete readings;
	delete readingSet;

}
