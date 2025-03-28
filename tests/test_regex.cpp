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
	void plugin_shutdown(PLUGIN_HANDLE handle);

	extern void Handler(void *handle, READINGSET *readings);
};

static const char *excludeTest = "{ \"rules\" : [ { \"asset_name\" : \"Pressure.*\", \"action\" : \"exclude\" } ] }";
static const char *includeTest = "{ \"rules\" : [ { \"asset_name\" : \"Camera_\\\\d\", \"action\" : \"include\" } ], \"defaultAction\" : \"exclude\" }";
static const char *renameTest = "{ \"rules\" : [ { \"asset_name\" : \"^Tank.*\", \"action\" : \"rename\", \"new_asset_name\" : \"new\" } ] }";
static const char *dpmapTest = "{ \"rules\" : [ { \"asset_name\" : \".*Camera$\", \"action\" : \"datapointmap\", \"map\" : { \"ISO\" : \"Light Sensitivity\" } } ] }";
static const char *removeTest = "{ \"rules\" : [ { \"asset_name\" : \".*\", \"action\" : \"remove\", \"datapoint\" : \"value\" } ] }";
static const char *removeTest2 = "{ \"rules\" : [ { \"asset_name\" : \".*\", \"action\" : \"remove\", \"datapoint\" : \".*scale\" } ] }";

// Regular expression checked for
// .*: Matches 0 or more occurrences
TEST(ASSET_REGEX, exclude)
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
	Datapoint *value = new Datapoint("P1", dpv);
	readings->push_back(new Reading("Pressure", value));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("P1", dpv1);
	readings->push_back(new Reading("Pressure", value1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("P2", dpv2);
	readings->push_back(new Reading("PressurePump", value2));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	// asset_name = "Pressure.*" in rule matches both the assets Pressure and PressurePump. So both the assets have been excluded
	ASSERT_EQ(results.size(), 0);  

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

// Regular expression checked for
// \d: Matches any digit (equivalent to [0-9])
TEST(ASSET_REGEX, include)
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
	Datapoint *value = new Datapoint("feed", dpv);
	readings->push_back(new Reading("Camera", value));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("feed", dpv1);
	readings->push_back(new Reading("Camera", value1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("Floor1_Feed", dpv2);
	readings->push_back(new Reading("Camera_1", value2));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	// asset_name = "Camera_\\d" in rule matches asset which has a digit in last. That's why only Camera_1 has been included
	ASSERT_EQ(results.size(), 1);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "Camera_1");
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "Floor1_Feed");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1140);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

// Regular expression checked for
// ^: Matches the start of a line or string
TEST(ASSET_REGEX, rename)
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
	Datapoint *value = new Datapoint("temperature", dpv);
	readings->push_back(new Reading("Tank", value));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("temperature", dpv1);
	readings->push_back(new Reading("Tank", value1));

	testValue = 1140;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("temperature", dpv2);
	readings->push_back(new Reading("PressureTank", value2));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 3);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "new"); // asset_name = "^Tank" in rule matches asset which starts with Tank, Hence renamed.
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "temperature");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1000);

	out = results[1];
	ASSERT_STREQ(out->getAssetName().c_str(), "new"); // asset_name = "^Tank.*" in rule matches asset which starts with Tank, Hence renamed.
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "temperature");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1001);

	out = results[2];
	ASSERT_STREQ(out->getAssetName().c_str(), "PressureTank"); // asset_name = "^Tank.*" in rule matches asset which starts with Tank, Hence nort renamed.
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "temperature");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1140);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

// Regular expression checked for
// $: Matches the end of a line or string
TEST(ASSET_REGEX, datapointmap)
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
	Datapoint *value = new Datapoint("ISO", dpv);
	readings->push_back(new Reading("Floor1_Camera", value));

	testValue = 1001;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("ISO", dpv1);
	readings->push_back(new Reading("Camera_site1", value1));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	// That's PressureTank has been not been renamed
	ASSERT_EQ(results.size(), 2);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "Floor1_Camera");
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	// asset_name = ".*Camera$" in rule matches asset which ends with Camera. datapoint name ISO mapped to "Light Sensitivity"
	ASSERT_STREQ(outdp->getName().c_str(), "Light Sensitivity"); 
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1000);

	out = results[1];
	ASSERT_STREQ(out->getAssetName().c_str(), "Camera_site1");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	// asset_name = ".*Camera$" in rule matches asset which ends with Camera. datpoint ISO name didn't mapped to "Light Sensitivity"
	ASSERT_STREQ(outdp->getName().c_str(), "ISO");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1001);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

// Regular expression checked for
// .*: Matches 0 or more occurrences
TEST(ASSET_REGEX, remove)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", removeTest);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	vector<Datapoint *> dpVec;
	long testValue = 1000;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("value", dpv1);
	dpVec.emplace_back(value1);

	testValue = 1001;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("scale", dpv2);
	dpVec.emplace_back(value2);
	readings->push_back(new Reading("Pressure", dpVec));

	testValue = 1140;
	DatapointValue dpv3(testValue);
	Datapoint *value3 = new Datapoint("scale", dpv3);
	readings->push_back(new Reading("Humidity", value3));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 2);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "Pressure");
	ASSERT_EQ(out->getDatapointCount(), 1); // asset_name = ".*" in rule matches all the asset names. That's why datapoint "value" is removed
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "scale");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1001);

	// Humidty asset doesn't have datapoint "value". So nothing removed
	out = results[1];
	ASSERT_STREQ(out->getAssetName().c_str(), "Humidity");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "scale");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1140);
	
	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}

// Regular expression checked for
// .*: Matches 0 or more occurrences
TEST(ASSET_REGEX, remove2)
{
	PLUGIN_INFORMATION *info = plugin_info();
	ConfigCategory *config = new ConfigCategory("asset", info->config);
	ASSERT_NE(config, (ConfigCategory *)NULL);
	config->setItemsValueFromDefault();
	ASSERT_EQ(config->itemExists("config"), true);
	config->setValue("config", removeTest2);
	config->setValue("enable", "true");
	ReadingSet *outReadings;
	void *handle = plugin_init(config, &outReadings, Handler);
	vector<Reading *> *readings = new vector<Reading *>;

	vector<Datapoint *> dpVec;
	long testValue = 1000;
	DatapointValue dpv1(testValue);
	Datapoint *value1 = new Datapoint("value", dpv1);
	dpVec.emplace_back(value1);

	testValue = 1001;
	DatapointValue dpv2(testValue);
	Datapoint *value2 = new Datapoint("Pressure_scale", dpv2);
	dpVec.emplace_back(value2);
	readings->push_back(new Reading("Pressure", dpVec));

	vector<Datapoint *> dpVec1;
	testValue = 1140;
	DatapointValue dpv3(testValue);
	Datapoint *value3 = new Datapoint("Humidity_scale", dpv3);
	dpVec1.emplace_back(value3);

	testValue = 1200;
	DatapointValue dpv4(testValue);
	Datapoint *value4 = new Datapoint("value", dpv4);
	dpVec1.emplace_back(value4);

	readings->push_back(new Reading("Humidity", dpVec1));

	ReadingSet *readingSet = new ReadingSet(readings);
	delete readings;
	plugin_ingest(handle, (READINGSET *)readingSet);

	vector<Reading *> results = outReadings->getAllReadings();
	ASSERT_EQ(results.size(), 2);
	Reading *out = results[0];
	ASSERT_STREQ(out->getAssetName().c_str(), "Pressure");
	// asset_name = ".*" in rule matches all the asset names.
	// And datapoint = ".*scale" in rule matches all the datapoints which have scale word in last
	// That's why datapoint "Pressure_scale" is removed
	ASSERT_EQ(out->getDatapointCount(), 1);
	vector<Datapoint *> points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	Datapoint *outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "value");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1000);

	// asset_name = ".*" in rule matches all the asset names.
	// And datapoint = ".*scale" in rule matches all the datapoints which have scale word in last
	// That's why datapoint "Humidity_scale" is removed
	out = results[1];
	ASSERT_STREQ(out->getAssetName().c_str(), "Humidity");
	ASSERT_EQ(out->getDatapointCount(), 1);
	points = out->getReadingData();
	ASSERT_EQ(points.size(), 1);
	outdp = points[0];
	ASSERT_STREQ(outdp->getName().c_str(), "value");
	ASSERT_EQ(outdp->getData().getType(), DatapointValue::T_INTEGER);
	ASSERT_EQ(outdp->getData().toInt(), 1200);

	delete outReadings;
	plugin_shutdown(handle);
	delete config;
}
