^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package gazebo_video_monitor_plugins
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

0.4.2 (2020-08-30)
-----------
* Enable C++14

0.4.1 (2020-08-21)
-----------
* Fix cmake and compiler warnings

0.4.0 (2020-08-15)
-----------
* Add multi view monitor plugin
  Support quadrant camera streams in the video recorder
* Add license notice

0.3.0 (2020-08-13)
------------------
* Various fixes
  * Fix bug with reading the cameraReference configurations
  * Fix success field on result when discarding in stop recording
  * Add a walking actor in video monitor plugin world as a dynamic element
  * Expose recorder library
* Add multi video monitor plugin
* Add multi camera monitor plugin
  * Add camera contains plugin and box marker visualizer
  * Add test world for multi camera monitor and camera contains plugins
  * Use default video name in the recorder when one is not provided
  * Fix gazebo topic names for camera images in multicamera sensor

0.2.0 (2020-06-29)
------------------
* Parameterize log prefixes
* Drop specific OpenCV version
* Refactor gazebo video monitor plugin
  * Introduce gazebo video recorder to host recording functionality and enable reusability
  * Introduce gazebo monitor base plugin to host common members and structure plugin initialization
  * Add option to disable the window in the gazebo video monitor plugin
  * Update documentation

0.1.1 (2020-04-23)
------------------
* Set path of temporary recording
* Add fix for initial camera attachment

0.1.0 (2020-04-21)
------------------
* Add multicamera sensor and video monitor plugin
