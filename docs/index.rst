.. Images
.. |asset| image:: images/asset.jpg


Asset Filter
============

The *fledge-filter-asset* is a filter provides a means to alter the flow of assets and datapoints as the data traverses a pipeline. It does not change the values in anyway, but it does allow for the renaming and removal of assets or datapoints within assets. It can also be used to flatten a hierarchical datapoint structure for use with destinations that are unable to handle hierarchical data.

Datapoints may be removed from the stream based on the type of the datapoint, this is useful to remove complex types that elements upstream in the pipeline are unable to handle, for example to remove image data before the stream is sent to a destination that is unable to handle images.

It may be used either in *South* or *North* services or *North* tasks and is driven by a set of rules that defines what actions to take. Multiple rules may be combined within a single invocation of the filter.

Asset filters are added in the same way as any other filters.

  - Click on the Applications add icon for your service or task.

  - Select the *asset* plugin from the list of available plugins.

  - Name your asset filter.

  - Click *Next* and you will be presented with the following configuration page

+---------+
| |asset| |
+---------+

  - Enter the *Asset rules*

  - Enable the plugin and click *Done* to activate it

Asset Rules
-----------

The asset rules are an array of JSON requires **rules** as an array of objects which define the asset name to which the rule is applied and an action. Rules on an asset will be executed in the exact order defined in the JSON array. Actions can be one of

  - **include**: The asset should be forwarded to the output of the filter

  - **exclude**: The asset should not be forwarded to the output of the filter

  - **rename**: Change the name of the asset. In this case a third property is included in the rule object, "new_asset_name"

  - **remove**: This action will be passed a datapoint name as an argument or a datapoint type. A datapoint with that name will be removed from the asset as it passed through the asset filter. If a type is passed then all data points of that type will be removed.

    .. list-table:: Supported data types for "remove" action
       :header-rows: 1

       * - Data type
         - Details
       * - INTEGER
         - Integer number 
       * - FLOATING 
         - Maps to FLOAT
       * - NUMBER 
         - Both integer and floating point values
       * - NON-NUMERIC
         - Everything except integer and floating point values
       * - STRING 
         - String of characters
       * - DP_LIST
         - Datapoint list 
       * - DP_DICT
         - Datapoint dictionary 
       * - IMAGE
         - Image 
       * - FLOAT_ARRAY 
         - Float array
       * - 2D_FLOAT_ARRAY 
         - Two dimensional float array
       * - BUFFER 
         - Maps to DATABUFFER
       * - ARRAY 
         - FLOAT_ARRAY datapoints
       * - 2D_ARRAY
         - FLOAT_ARRAY datapoints
       * - USER_ARRAY 
         - Both FLOAT_ARRAY and 2D_FLOAT_ARRAY datapoints
       * - Nested 
         - DP_DICT

    .. note::

        Datapoint types are case insensitive.

  - **select**: Select the datapoints that should be forwarded by the filter to the next stage of the pipeline. The action will be passed a list of datapoint names that will be sent forwards.

  - **split**: This action will allow to split an asset into multiple assets.

  - **flatten**: This action will flatten nested datapoint structure to single level. 

  - **datapointmap**: Map the names of the datapoints within the asset. In this case a third property is included in the rule object, "map". This is an object that maps the current names of the data points to new names.

In addition a *defaultAction* may be included, however this is limited to *include*, *exclude* and *flatten*. Any asset that does not match a specific rule will have this default action applied to them. If the default action it not given it is treated as if a default action of *include* had been set.

Examples
--------

The following are some examples of how the *asset* filter may be used.

Remove Assets From The Pipeline
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We wish to remove the asset called *raw* from the pipeline, for example if this asset has been used in calculations earlier in the pipeline and is now no longer needed. We can use a rule

.. code-block:: JSON

   {
      "rules" : [
                  {
                     "asset_name" : "raw",
                     "action"     : "exclude"
                  }
                ]
   }

As the default is to leave any unmatched asset unaltered in the pipeline the above rule will not impact assets other than *raw*.

We can change the default action, as an alternative lets saw we use multiple assets in the pipeline to calculate a new asset called *quality*, we want to remove the assets used to calculate *quality* but do not wish to name each of them. In this case we can use a rule

.. code-block:: JSON

   {
      "rules" : [
                  {
                     "asset_name" : "quality",
                     "action"     : "include"
                  }
                ],
      "defaultAction" : "exclude"
   }

Since we have used the *defaultAction* with *exclude*, and asset that does not match the rules above will be removed from the pipeline.

Flatten Hierarchical Data
~~~~~~~~~~~~~~~~~~~~~~~~~

Flatten a hierarchy datapoint called *pressure* that has three children, *floor1*, *floor2* and *floor3* within an asset called *water*.

.. code-block:: JSON

  {
      "pressure": { "floor1" : 30, "floor2" : 34, "floor3" : 36 }
  }

We can use the rule

.. code-block:: JSON

   {
      "rules" : [
                  {
                     "asset_name" : "water",
                     "action"     : "flatten"
                  }
                ]
   }

The datapoint *pressure* will be flattened and three new data points will be created,  *pressure_floor1*, *pressure_floor2* and *pressure_floor3*. The resultant asset will no longer have the hierarchical datapoint *pressure* included within it.

Changing Datapoint Names
~~~~~~~~~~~~~~~~~~~~~~~~

Using a map to change the names of the datapoints within an asset.

Given an asset with the datapoints *rpm*, *X* and *depth* we want to rename them to be *motorSpeed*, *toolOffset* and *curDepth*. We use a *map* as follows to accomplish this.

.. code-block:: JSON

  {
      "rules" : [
                   {
                      "asset_name" : "lathe328",
                      "action"     : "datapointmap",
                      "map"        : {
                                        "rpm"   : "motorSpeed",
                                        "X"     : "toolOffset",
                                        "depth" : "cutDepth"
                                     }
                   }
                ]
  }
 
This map will transform the asset as follows

.. list-table:: Map example
   :header-rows: 1

   * - Existing Datapoint name
     - New Datapoint Name
   * - rpm
     - motorSpeed
   * - X 
     - toolOffset
   * - depth 
     - cutDepth

Remove Named Datapoint From An Asset
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Suppose we have a vibration sensor that gives us three datapoints for the vibration, *X*, *Y* and *Z*. We use the *expression* filter earlier in the pipeline to add a new combined vector for the vibration and we now wish to remove the *X*, *Y* and *Z* datapoints. We can do this with the asset filter by uses a set of rules as follows.

.. code-block:: JSON

   {
      "rules" : [
                  {
                     "asset_name" : "vibration",
                     "action"     : "remove",
                     "datapoint"  : "X"
                  },
                  {
                     "asset_name" : "vibration",
                     "action"     : "remove",
                     "datapoint"  : "Y"
                  },
                  {
                     "asset_name" : "vibration",
                     "action"     : "remove",
                     "datapoint"  : "Z"
                  }
                ]
   }

Passing On A Subset Of Datapoints
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Using the same vibration sensor as above, but we only want to include the *X* and *Y* components of vibration. We can filter out the other components, and any other datapoints that might appear in the pipeline by using the *select* action

.. code-block:: JSON

   {
      "rules" : [
                  {
                     "asset_name" : "vibration",
                     "action"     : "select",
                     "datapoints" : [ "X", "Y" ]
                  }
                ]
   }

We could accomplish the removal of the *Z* datapoint by using the remove action,

.. code-block:: JSON

   {
      "rules" : [
                  {
                     "asset_name" : "vibration",
                     "action"     : "remove",
                     "datapoint"  : "Z"
                  }
                ]
   }

However the *select* action has the added benefit if other datapoints were to appear in the pipeline they would be blocked by this action.

.. note::

   If a reading is missing one or more of the datapoints in the select actions *datapoints* list then only those datapoints that exist in the reading and the *datapoints* list will be passed onwards in the pipeline. No error or warning will be raised by the asset filter for missing datapoints.

Removing Image Data From Pipelines
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this example we have a pipeline that ingests images from a camera, passes them through image processing filters and a computer vision filter that produces metrics based on the image content. We want to send those metric to upstream systems but these systems do not support image data. We can use the *asset* filter to remove all image type datapoints from the pipeline.

.. code-block:: JSON

   {
      "rules" : [
                  {
                     "asset_name" : "camera1",
                     "action"     : "remove",
                     "type"       : "image"
                  }
                ]
   }

Split an asset into multiple assets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this example an asset named **lathe1014** will be splited into muliple assets **asset1**, **asset2** and **asset3**.

* New asset **asset1** will have datapoints **a**, **b** and **f** from asset **lathe1014**

* New asset **asset2** will have datapoints **a**, **e** and **g** from asset **lathe1014**

* New asset **asset3** will have datapoints **b** and **d** from asset **lathe1014**

.. code-block:: JSON

   {
      "rules" : [
                  {
                     "asset_name" : "lathe1014",
                     "action"     : "split",
                     "split"      : {
                        "asset1" : [ "a", "b", "f"],
                        "asset2" : [ "a", "e", "g"],
                        "asset3" : [ "b", "d"]
                      }
                  }
                ]
   }

Note: If **split** key is missing then one new asset per datapoint will be created. The name of new asset will be the original asset name with the datapoint name appended following an underscore separator.

Combining Rules
~~~~~~~~~~~~~~~

Rules may be combined to perform multiple operations in a single stage of a pipeline, the following example shows such a situation.

.. code-block:: JSON

  {
	"rules": [
                   {
			"asset_name": "Random1",
			"action": "include"
		   },
                   {
			"asset_name": "Random2",
			"action": "rename",
			"new_asset_name": "Random92"
		   },
                   {
			"asset_name": "Random3",
			"action": "exclude"
		   },
                   {
			"asset_name": "Random4",
			"action": "rename",
			"new_asset_name": "Random94"
		   },
                   {
			"asset_name": "Random5",
			"action": "exclude"
		   },
                   {
			"asset_name": "Random6",
			"action": "rename",
			"new_asset_name": "Random96"
		   },
                   {
			"asset_name": "Random7",
			"action": "include"
	           },
              {
			"asset_name": "Random8",
			"action": "flatten"
	           },
                   {
                        "asset_name": "lathe1004",
                        "action": "datapointmap",
                        "map": {
                                "rpm": "motorSpeed",
                                "X": "toolOffset",
                                "depth": "cutDepth"
                        }
                   },
                   {
                        "asset_name": "Random6",
                        "action": "remove",
                        "datapoint": "sinusoid_7"
                   },
                   {
                        "asset_name": "Random6",
                        "action": "remove",
                        "type": "FLOAT"
                   }
        ],
	"defaultAction": "include"
  }

Regular Expression
~~~~~~~~~~~~~~~~~~

Regular expression can be used for asset_name values in the JSON; datapoint values with remove action can also use regular expression.
In the following example, Any datapoint which starts with "Pressure" will be removed from all the assets; if exists.

.. code-block:: JSON

  {
	"rules": [
      
          {
              "asset_name": ".*",
              "action": "remove",
              "datapoint": "Pressure.*"
          }
        ],
	"defaultAction": "include"
  }



The filter supports the standard Linux regular expression syntax

.. list-table::
   :widths: 10 90
   :header-rows: 1

   * - Expression
     - Description
   * - \.
     - Matches any character
   * - \[a-z]
     - Matches any characters in the range between the two given
   * - \*
     - Matches zero or more occurrences of the previous item
   * - \+
     - Matches one or more occurrence of the previous item
   * - \?
     - Matches zero or one occurrence of the previous item
   * - ^
     - Matches the start of the string
   * - \$
     - Matches the end of the string
   * - \d
     - Matches any digit (equivalent to [0-9])

Examples
~~~~~~~~

To match a word, defined as one or more letters, we can use the regular expression

.. code-block:: console

   [A-Za-z].*

If we wanted to match capitalised words only then we could use

.. code-block:: Console

   [A-Z].*
   
If we wanted to match only words starting with an *a* or *b* character there are a number of ways we could do this   

.. code-block:: console

    [ab][a-z].*

or

.. code-block:: console

    a|b[a-z].*

If we wanted to match the words staring with *Tank* we can use the ^ operator

.. code-block:: console

    ^Tank
    
If we wanted to match the words *spark* and *sparks* we can use the ? operator

.. code-block:: console

    spark.?
    
If we wanted to match the words *camera_1* we can use the d operator

.. code-block:: console

   camera_\\d
   
The above are a few examples of regular expressions that can be used, but serve to illustrate the most used operators that are available.

