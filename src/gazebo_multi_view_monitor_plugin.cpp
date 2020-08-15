#include <ctime>

#include <gazebo_video_monitor_plugins/gazebo_multi_view_monitor_plugin.h>
#include <gazebo_video_monitor_plugins/internal/utils.h>

namespace gazebo {

GazeboMultiViewMonitorPlugin::GazeboMultiViewMonitorPlugin()
    : GazeboMonitorBasePlugin(getClassName<GazeboMultiViewMonitorPlugin>()) {
  node_ = boost::make_shared<transport::Node>();
  node_->Init();
}

GazeboMultiViewMonitorPlugin::~GazeboMultiViewMonitorPlugin() {
  recorder_->reset();
}

void GazeboMultiViewMonitorPlugin::Load(sensors::SensorPtr _sensor,
                                        sdf::ElementPtr _sdf) {
  GazeboMonitorBasePlugin::Load(_sensor, _sdf);

  // Get recorder config
  if (not sdf_->HasElement("recorder"))
    gzthrow(logger_prefix_ + "Failed to get recorder");
  auto sdf_recorder = sdf_->GetElement("recorder");

  // Confirm presence of cameras
  auto names = sensor_->getCameraNames();
  if (names.empty())
    gzthrow(logger_prefix_ << "There are no cameras in the sensor");

  // Initialize cameras configuration
  for (size_t i = 0; i < names.size(); ++i)
    camera_name_to_index_map_[names[i]] = i;
  camera_indices_ = std::vector<size_t>(4, std::numeric_limits<size_t>::max());

  // Initialize camera select gazebo subscriber
  camera_select_subscriber_ = node_->Subscribe(
      "~/" + sdf_->Get<std::string>("name") + "/camera_select",
      &GazeboMultiViewMonitorPlugin::cameraSelectCallback, this);

  // Initialize recorder
  recorder_ = std::make_shared<GazeboVideoRecorder>(
      static_cast<unsigned int>(sensor_->UpdateRate()),
      getClassName<GazeboMultiViewMonitorPlugin>());
  if (not sdf_->HasElement("recorder"))
    gzthrow(logger_prefix_ + "Failed to get recorder");
  recorder_->load(world_, sdf_->GetElement("recorder"));
}

void GazeboMultiViewMonitorPlugin::Reset() {
  std::lock_guard<std::mutex> lock(recorder_mutex_);
  if (sensor_->isRecording()) stopRecording(true);
}

void GazeboMultiViewMonitorPlugin::initRos() {
  GazeboMonitorBasePlugin::initRos();

  // Get start recording service name
  if (not sdf_->HasElement("startRecordingService"))
    gzthrow(logger_prefix_ + "Failed to get startRecordingService");
  auto start_service_name = sdf_->Get<std::string>("startRecordingService");

  // Get stop recording service name
  if (not sdf_->HasElement("stopRecordingService"))
    gzthrow(logger_prefix_ + "Failed to get stopRecordingService");
  auto stop_service_name = sdf_->Get<std::string>("stopRecordingService");

  // Get camera select topic name
  if (not sdf_->HasElement("cameraSelectTopic"))
    gzthrow(logger_prefix_ + "Failed to get cameraSelectTopic");
  auto camera_select_topic_name = sdf_->Get<std::string>("cameraSelectTopic");

  // Initialize recording services
  start_recording_service_ = nh_->advertiseService(
      start_service_name,
      &GazeboMultiViewMonitorPlugin::startRecordingServiceCallback, this);
  stop_recording_service_ = nh_->advertiseService(
      stop_service_name,
      &GazeboMultiViewMonitorPlugin::stopRecordingServiceCallback, this);

  // Initialize camera select subscriber
  camera_select_ros_subscriber_ = nh_->subscribe(
      camera_select_topic_name, 10,
      &GazeboMultiViewMonitorPlugin::cameraSelectRosCallback, this);
}

void GazeboMultiViewMonitorPlugin::onNewImages(
    const ImageDataPtrVector &images) {
  std::unique_lock<std::mutex> lock(recorder_mutex_, std::try_to_lock);
  if (not sensor_->isRecording() or not lock.owns_lock()) return;

  recorder_->addMultiViewFrame(getImage(images, 0), getImage(images, 1),
                               getImage(images, 2), getImage(images, 3));
}

const GazeboMonitorBasePlugin::ImageDataPtrVector::value_type &
GazeboMultiViewMonitorPlugin::getImage(
    const GazeboMonitorBasePlugin::ImageDataPtrVector &images, size_t i) const {
  return camera_indices_[i] == std::numeric_limits<size_t>::max()
             ? image_null_
             : images[camera_indices_[i]];
}

void GazeboMultiViewMonitorPlugin::cameraSelect(
    const std::vector<std::string> &names) {
  if (names.size() > 4)
    ROS_WARN_STREAM(logger_prefix_
                    << "Received message with more than 4 camera names; "
                       "ignoring the extra cameras");

  for (size_t i = 0; i < camera_indices_.size(); ++i) {
    if (i < names.size() and camera_name_to_index_map_.count(names[i]) > 0)
      camera_indices_[i] = camera_name_to_index_map_[names[i]];
    else
      camera_indices_[i] = std::numeric_limits<size_t>::max();
  }
}

void GazeboMultiViewMonitorPlugin::cameraSelectCallback(
    const boost::shared_ptr<const ignition::msgs::StringMsg_V> &msg) {
  std::vector<std::string> names;
  for (int i = 0; i < msg->data_size(); ++i) names.push_back(msg->data(i));
  std::lock_guard<std::mutex> lock(recorder_mutex_);
  cameraSelect(names);
}

void GazeboMultiViewMonitorPlugin::cameraSelectRosCallback(
    const gazebo_video_monitor_plugins::StringsConstPtr &msg) {
  std::lock_guard<std::mutex> lock(recorder_mutex_);
  cameraSelect(msg->names);
}

std::string GazeboMultiViewMonitorPlugin::stopRecording(bool discard,
                                                        std::string filename) {
  sensor_->setRecording(false);
  return recorder_->stop(discard, filename);
}

bool GazeboMultiViewMonitorPlugin::startRecordingServiceCallback(
    gazebo_video_monitor_plugins::StartGmcmRecordingRequest &req,
    gazebo_video_monitor_plugins::StartGmcmRecordingResponse & /*res*/) {
  std::lock_guard<std::mutex> lock(recorder_mutex_);

  // Stop active recording
  if (sensor_->isRecording()) {
    ROS_WARN_STREAM(logger_prefix_
                    << "There is already an active recording; resetting");
    stopRecording(true);
  }

  // Select cameras
  cameraSelect(req.cameras.names);

  // Start recording
  recorder_->start(save_path_, getStringTimestamp(std::time(nullptr)),
                   world_->RealTime());

  // Set state
  sensor_->setRecording(true);

  return true;
}

bool GazeboMultiViewMonitorPlugin::stopRecordingServiceCallback(
    gazebo_video_monitor_plugins::StopRecordingRequest &req,
    gazebo_video_monitor_plugins::StopRecordingResponse &res) {
  if (not sensor_->isRecording()) {
    ROS_WARN_STREAM(logger_prefix_ << "No active recording; ignoring request");
    res.success = false;
    return true;
  }

  std::lock_guard<std::mutex> lock(recorder_mutex_);
  res.path = stopRecording(req.discard, req.filename);
  res.success = not res.path.empty() or req.discard;
  return true;
}

GZ_REGISTER_SENSOR_PLUGIN(GazeboMultiViewMonitorPlugin)

}  // namespace gazebo
