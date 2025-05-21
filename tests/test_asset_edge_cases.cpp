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
#include <thread>
#include <vector>
#include <chrono>

using namespace std;
using namespace rapidjson;

extern "C" {
    PLUGIN_INFORMATION *plugin_info();
    void plugin_ingest(void *handle, READINGSET *readingSet);
    PLUGIN_HANDLE plugin_init(ConfigCategory* config,
                  OUTPUT_HANDLE *outHandle,
                  OUTPUT_STREAM output);
    void plugin_shutdown(PLUGIN_HANDLE handle);
    void plugin_reconfigure(void *handle, const string& newConfig);

    static void Handler(void *handle, READINGSET *readings)
    {
        *(READINGSET **)handle = readings;
    }
};

// Test configurations
static const char *specialAssetNamesConfig = QUOTE({
    "rules": [
        {
            "asset_name": "asset with spaces",
            "action": "rename",
            "new_asset_name": "renamed_asset"
        },
        {
            "asset_name": "asset@#$%^&*()",
            "action": "rename",
            "new_asset_name": "special_chars_removed"
        }
    ]
});

static const char *multipleActionsConfig = QUOTE({
    "rules": [
        {
            "asset_name": "test",
            "action": "rename",
            "new_asset_name": "renamed"
        },
        {
            "asset_name": "renamed",
            "action": "datapointmap",
            "map": {
                "value": "new_value"
            }
        }
    ]
});

static const char *complexNestingConfig = QUOTE({
    "rules": [
        {
            "asset_name": "nested",
            "action": "flatten"
        }
    ]
});

// Test configurations for additional test cases
static const char *regexEdgeCasesConfig = QUOTE({
    "rules": [
        {
            "asset_name": "test([0-9]+)",
            "action": "rename",
            "new_asset_name": "renamed$1"
        },
        {
            "asset_name": ".*",
            "action": "datapointmap",
            "map": {
                "value": "mapped_value"
            }
        }
    ]
});

static const char *rulePriorityConfig = QUOTE({
    "rules": [
        {
            "asset_name": "test",
            "action": "rename",
            "new_asset_name": "renamed1"
        },
        {
            "asset_name": "test",
            "action": "rename",
            "new_asset_name": "renamed2"
        },
        {
            "asset_name": "renamed.*",
            "action": "datapointmap",
            "map": {
                "value": "mapped_value"
            }
        }
    ]
});

static const char *pluginDisabledConfig = QUOTE({
    "rules": [
        {
            "asset_name": "test",
            "action": "rename",
            "new_asset_name": "renamed_asset"
        }
    ]
});

// Test with special asset names
TEST(ASSET_EDGE_CASES, SpecialAssetNames)
{
    PLUGIN_INFORMATION *info = plugin_info();
    ConfigCategory *config = new ConfigCategory("asset", info->config);
    ASSERT_NE(config, (ConfigCategory *)NULL);
    config->setItemsValueFromDefault();
    config->setValue("config", specialAssetNamesConfig);
    config->setValue("enable", "true");
    
    ReadingSet *outReadings;
    void *handle = plugin_init(config, &outReadings, Handler);
    
    // Test asset with spaces
    vector<Reading *> *readings = new vector<Reading *>;
    long testValue = 1000;
    DatapointValue dpv(testValue);
    Datapoint *value = new Datapoint("test", dpv);
    readings->push_back(new Reading("asset with spaces", value));
    
    ReadingSet *readingSet = new ReadingSet(readings);
    delete readings;
    plugin_ingest(handle, (READINGSET *)readingSet);
    
    vector<Reading *> results = outReadings->getAllReadings();
    ASSERT_EQ(results.size(), 1);
    ASSERT_STREQ(results[0]->getAssetName().c_str(), "renamed_asset");
    
    delete outReadings;
    plugin_shutdown(handle);
    delete config;
}

// Test multiple actions on same asset
TEST(ASSET_EDGE_CASES, MultipleActions)
{
    PLUGIN_INFORMATION *info = plugin_info();
    ConfigCategory *config = new ConfigCategory("asset", info->config);
    ASSERT_NE(config, (ConfigCategory *)NULL);
    config->setItemsValueFromDefault();
    config->setValue("config", multipleActionsConfig);
    config->setValue("enable", "true");
    
    ReadingSet *outReadings;
    void *handle = plugin_init(config, &outReadings, Handler);
    
    vector<Reading *> *readings = new vector<Reading *>;
    long testValue = 1000;
    DatapointValue dpv(testValue);
    Datapoint *value = new Datapoint("value", dpv);
    readings->push_back(new Reading("test", value));
    
    ReadingSet *readingSet = new ReadingSet(readings);
    delete readings;
    plugin_ingest(handle, (READINGSET *)readingSet);
    
    vector<Reading *> results = outReadings->getAllReadings();
    ASSERT_EQ(results.size(), 1);
    ASSERT_STREQ(results[0]->getAssetName().c_str(), "renamed");
    
    vector<Datapoint *> points = results[0]->getReadingData();
    ASSERT_EQ(points.size(), 1);
    ASSERT_STREQ(points[0]->getName().c_str(), "new_value");
    
    delete outReadings;
    plugin_shutdown(handle);
    delete config;
}

// Test complex nested datapoints
TEST(ASSET_EDGE_CASES, ComplexNesting)
{
    PLUGIN_INFORMATION *info = plugin_info();
    ConfigCategory *config = new ConfigCategory("asset", info->config);
    ASSERT_NE(config, (ConfigCategory *)NULL);
    config->setItemsValueFromDefault();
    config->setValue("config", complexNestingConfig);
    config->setValue("enable", "true");
    
    ReadingSet *outReadings;
    void *handle = plugin_init(config, &outReadings, Handler);
    
    // Create a complex nested structure
    vector<Reading *> *readings = new vector<Reading *>;
    
    // Create datapoints
    std::vector<Datapoint *> dataPoints;
    DatapointValue level1Value("value1");
    long level2Value = 2;
    DatapointValue level2Dpv(level2Value);
    dataPoints.push_back(new Datapoint("level1", level1Value));
    dataPoints.push_back(new Datapoint("level2", level2Dpv));
    
    readings->push_back(new Reading("nested", dataPoints));
    
    ReadingSet *readingSet = new ReadingSet(readings);
    delete readings;
    plugin_ingest(handle, (READINGSET *)readingSet);
    
    vector<Reading *> results = outReadings->getAllReadings();
    ASSERT_EQ(results.size(), 1);
    
    vector<Datapoint *> points = results[0]->getReadingData();
    ASSERT_EQ(points.size(), 2); // Should be flattened into two datapoints
    
    delete outReadings;
    plugin_shutdown(handle);
    delete config;
}

// Test regex edge cases
TEST(ASSET_EDGE_CASES, RegexEdgeCases)
{
    PLUGIN_INFORMATION *info = plugin_info();
    ConfigCategory *config = new ConfigCategory("asset", info->config);
    ASSERT_NE(config, (ConfigCategory *)NULL);
    config->setItemsValueFromDefault();
    config->setValue("config", regexEdgeCasesConfig);
    config->setValue("enable", "true");
    
    ReadingSet *outReadings;
    void *handle = plugin_init(config, &outReadings, Handler);
    
    vector<Reading *> *readings = new vector<Reading *>;
    
    // Test with matching regex
    long testValue1 = 1000;
    DatapointValue dpv1(testValue1);
    Datapoint *value1 = new Datapoint("value", dpv1);
    readings->push_back(new Reading("test123", value1));
    
    // Test with non-matching asset
    long testValue2 = 2000;
    DatapointValue dpv2(testValue2);
    Datapoint *value2 = new Datapoint("value", dpv2);
    readings->push_back(new Reading("other", value2));
    
    ReadingSet *readingSet = new ReadingSet(readings);
    delete readings;
    plugin_ingest(handle, (READINGSET *)readingSet);
    
    vector<Reading *> results = outReadings->getAllReadings();
    ASSERT_EQ(results.size(), 2);
    
    // Check first reading (should be renamed and mapped)
    ASSERT_STREQ(results[0]->getAssetName().c_str(), "renamed123");
    vector<Datapoint *> points1 = results[0]->getReadingData();
    ASSERT_EQ(points1.size(), 1);
    ASSERT_STREQ(points1[0]->getName().c_str(), "mapped_value");
    
    // Check second reading (should only be mapped)
    ASSERT_STREQ(results[1]->getAssetName().c_str(), "other");
    vector<Datapoint *> points2 = results[1]->getReadingData();
    ASSERT_EQ(points2.size(), 1);
    ASSERT_STREQ(points2[0]->getName().c_str(), "mapped_value");
    
    delete outReadings;
    plugin_shutdown(handle);
    delete config;
}

// Test rule priority
TEST(ASSET_EDGE_CASES, RulePriority)
{
    PLUGIN_INFORMATION *info = plugin_info();
    ConfigCategory *config = new ConfigCategory("asset", info->config);
    ASSERT_NE(config, (ConfigCategory *)NULL);
    config->setItemsValueFromDefault();
    config->setValue("config", rulePriorityConfig);
    config->setValue("enable", "true");
    
    ReadingSet *outReadings;
    void *handle = plugin_init(config, &outReadings, Handler);
    
    vector<Reading *> *readings = new vector<Reading *>;
    long testValue = 1000;
    DatapointValue dpv(testValue);
    Datapoint *value = new Datapoint("value", dpv);
    readings->push_back(new Reading("test", value));
    
    ReadingSet *readingSet = new ReadingSet(readings);
    delete readings;
    plugin_ingest(handle, (READINGSET *)readingSet);
    
    vector<Reading *> results = outReadings->getAllReadings();
    ASSERT_EQ(results.size(), 1);
    
    // Check that the first matching rule is applied
    ASSERT_STREQ(results[0]->getAssetName().c_str(), "renamed1");
    
    // Check that the datapoint mapping is applied after renaming
    vector<Datapoint *> points = results[0]->getReadingData();
    ASSERT_EQ(points.size(), 1);
    ASSERT_STREQ(points[0]->getName().c_str(), "mapped_value");
    
    delete outReadings;
    plugin_shutdown(handle);
    delete config;
}

// Test data type preservation
TEST(ASSET_EDGE_CASES, DataTypePreservation)
{
    PLUGIN_INFORMATION *info = plugin_info();
    ConfigCategory *config = new ConfigCategory("asset", info->config);
    ASSERT_NE(config, (ConfigCategory *)NULL);
    config->setItemsValueFromDefault();
    config->setValue("config", "{}");
    config->setValue("enable", "true");
    
    ReadingSet *outReadings;
    void *handle = plugin_init(config, &outReadings, Handler);
    
    vector<Reading *> *readings = new vector<Reading *>;
    
    // Test with different data types
    long intValue = 42;
    DatapointValue intDpv(intValue);
    Datapoint *intDp = new Datapoint("int_value", intDpv);
    
    double floatValue = 42.5;
    DatapointValue floatDpv(floatValue);
    Datapoint *floatDp = new Datapoint("float_value", floatDpv);
    
    DatapointValue stringValue("test");
    Datapoint *stringDp = new Datapoint("string_value", stringValue);
    
    readings->push_back(new Reading("test", {intDp, floatDp, stringDp}));
    
    ReadingSet *readingSet = new ReadingSet(readings);
    delete readings;
    plugin_ingest(handle, (READINGSET *)readingSet);
    
    vector<Reading *> results = outReadings->getAllReadings();
    ASSERT_EQ(results.size(), 1);
    
    vector<Datapoint *> points = results[0]->getReadingData();
    ASSERT_EQ(points.size(), 3);
    
    // Verify data types are preserved
    ASSERT_EQ(points[0]->getData().getType(), DatapointValue::T_INTEGER);
    ASSERT_EQ(points[1]->getData().getType(), DatapointValue::T_FLOAT);
    ASSERT_EQ(points[2]->getData().getType(), DatapointValue::T_STRING);
    
    delete outReadings;
    plugin_shutdown(handle);
    delete config;
}

// Test plugin behavior when disabled
TEST(ASSET_EDGE_CASES, PluginDisabled)
{
    PLUGIN_INFORMATION *info = plugin_info();
    ConfigCategory *config = new ConfigCategory("asset", info->config);
    ASSERT_NE(config, (ConfigCategory *)NULL);
    config->setItemsValueFromDefault();
    config->setValue("config", pluginDisabledConfig);
    config->setValue("enable", "false");  // Disable the plugin
    
    ReadingSet *outReadings;
    void *handle = plugin_init(config, &outReadings, Handler);
    
    // Create a reading with a value that would normally be transformed
    vector<Reading *> *readings = new vector<Reading *>;
    long testValue = 1000;
    DatapointValue dpv(testValue);
    Datapoint *value = new Datapoint("value", dpv);
    readings->push_back(new Reading("test", value));
    
    ReadingSet *readingSet = new ReadingSet(readings);
    delete readings;
    plugin_ingest(handle, (READINGSET *)readingSet);
    
    // Verify the reading passes through unaltered despite having a rename rule
    vector<Reading *> results = outReadings->getAllReadings();
    ASSERT_EQ(results.size(), 1);
    ASSERT_STREQ(results[0]->getAssetName().c_str(), "test");  // Should not be renamed to "renamed_asset"
    
    vector<Datapoint *> points = results[0]->getReadingData();
    ASSERT_EQ(points.size(), 1);
    ASSERT_STREQ(points[0]->getName().c_str(), "value");
    ASSERT_EQ(points[0]->getData().getType(), DatapointValue::T_INTEGER);
    ASSERT_EQ(points[0]->getData().toInt(), testValue);
    
    delete outReadings;
    plugin_shutdown(handle);
    delete config;
} 