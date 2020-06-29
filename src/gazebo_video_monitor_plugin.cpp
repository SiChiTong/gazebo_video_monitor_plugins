#include <ctime>

#include <gazebo_video_monitor_plugins/gazebo_video_monitor_plugin.h>
#include <gazebo_video_monitor_plugins/internal/utils.h>

namespace gazebo {

GazeboVideoMonitorPlugin::GazeboVideoMonitorPlugin()
    : GazeboMonitorBasePlugin(getClassName<GazeboVideoMonitorPlugin>()),
      camera_names_({"world_camera", "robot_camera"}) {}

GazeboVideoMonitorPlugin::~GazeboVideoMonitorPlugin() { recorder_.reset(); }

void GazeboVideoMonitorPlugin::Load(sensors::SensorPtr _sensor,
                                    sdf::ElementPtr _sdf) {
  GazeboMonitorBasePlugin::Load(_sensor, _sdf);

  // Confirm cameras
  if (sensor_->getCameraNames() != camera_names_)
    gzthrow(logger_prefix_ + "Wrong cameras configuration; please " +
            "provide two cameras with names: " + toString(camera_names_));

  // Confirm camera reference config for robot camera
  if (not getCameraRefConfig(camera_names_[1]))
    gzwarn << logger_prefix_ << camera_names_[1]
           << " camera reference configuration is not provided\n";

  // Initialize recorder
  recorder_ = std::make_shared<GazeboVideoRecorder>(
      static_cast<unsigned int>(sensor_->UpdateRate()),
      getClassName<GazeboVideoMonitorPlugin>());
  if (not sdf_->HasElement("recorder"))
    gzthrow(logger_prefix_ + "Failed to get recorder");
  recorder_->load(world_, sdf_->GetElement("recorder"));
}

void GazeboVideoMonitorPlugin::Reset() {
  std::lock_guard<std::mutex> lock(recorder_mutex_);
  if (sensor_->isRecording()) stopRecording(true);
}

void GazeboVideoMonitorPlugin::initRos() {
  GazeboMonitorBasePlugin::initRos();

  // Get start recording service name
  if (not sdf_->HasElement("startRecordingService"))
    gzthrow(logger_prefix_ + "Failed to get startRecordingService");
  auto start_service_name = sdf_->Get<std::string>("startRecordingService");

  // Get stop recording service name
  if (not sdf_->HasElement("stopRecordingService"))
    gzthrow(logger_prefix_ + "Failed to get stopRecordingService");
  auto stop_service_name = sdf_->Get<std::string>("stopRecordingService");

  // Initialize recording services
  start_recording_service_ = nh_->advertiseService(
      start_service_name,
      &GazeboVideoMonitorPlugin::startRecordingServiceCallback, this);
  stop_recording_service_ = nh_->advertiseService(
      stop_service_name,
      &GazeboVideoMonitorPlugin::stopRecordingServiceCallback, this);
}

void GazeboVideoMonitorPlugin::onNewImages(const ImageDataPtrVector &images) {
  std::unique_lock<std::mutex> lock(recorder_mutex_, std::try_to_lock);
  if (not sensor_->isRecording() or not lock.owns_lock()) return;

  if (world_as_main_view_)
    recorder_->addFrame(images[0], disable_window_ ? nullptr : images[1]);
  else
    recorder_->addFrame(images[1], disable_window_ ? nullptr : images[0]);
}

std::string GazeboVideoMonitorPlugin::stopRecording(bool discard,
                                                    std::string filename) {
  sensor_->setRecording(false);
  return recorder_->stop(discard, filename);
}

bool GazeboVideoMonitorPlugin::startRecordingServiceCallback(
    gazebo_video_monitor_plugins::StartGvmRecordingRequest &req,
    gazebo_video_monitor_plugins::StartGvmRecordingResponse & /*res*/) {
  std::lock_guard<std::mutex> lock(recorder_mutex_);

  // Stop active recording
  if (sensor_->isRecording()) {
    ROS_WARN_STREAM(logger_prefix_
                    << "There is already an active recording; resetting");
    stopRecording(true);
  }

  // Start recording
  recorder_->start(save_path_, getStringTimestamp(std::time(nullptr)),
                   world_->RealTime());

  // Set state
  disable_window_ = req.disable_window;
  world_as_main_view_ = req.world_as_main_view;
  sensor_->setRecording(true);

  return true;
}

bool GazeboVideoMonitorPlugin::stopRecordingServiceCallback(
    gazebo_video_monitor_plugins::StopRecordingRequest &req,
    gazebo_video_monitor_plugins::StopRecordingResponse &res) {
  if (not sensor_->isRecording()) {
    ROS_WARN_STREAM(logger_prefix_ << "No active recording; ignoring request");
    res.success = false;
    return true;
  }

  std::lock_guard<std::mutex> lock(recorder_mutex_);
  res.path = stopRecording(req.discard, req.filename);
  res.success = not res.path.empty();
  return true;
}

GZ_REGISTER_SENSOR_PLUGIN(GazeboVideoMonitorPlugin)

}  // namespace gazebo
