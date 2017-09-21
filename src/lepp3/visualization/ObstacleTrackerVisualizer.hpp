#ifndef LEPP3_VISUALIZATION_OBSTACLE_TRACKER_H__
#define LEPP3_VISUALIZATION_OBSTACLE_TRACKER_H__

#include "lepp3/visualization/BaseVisualizer.hpp"
#include "lepp3/Typedefs.hpp"
//#include "lepp3/GMMObstacleTrackerAggregator.hpp"
//#include "lepp3/GMMObstacleTrackerState.hpp"
#include "lepp3/util/VoxelGrid3D.h"
#include "lepp3/obstacles/segmenter/gmm/GmmData.hpp"

extern bool g_exitProgram;
extern bool g_enableObstacleTrackerRecorder;

namespace lepp {

namespace {
  enum class Ui_Option {
    ENABLE_TRACKER,
    ENABLE_TIGHT_FIT,
    DRAW_GAUSSIAN,
    DRAW_SSV,
    DRAW_TRAJECTORY,
    DRAW_VELOCIY,
    DRAW_DEBUG_VALUE,
    DRAW_VOXEL,
    COLOR_MODE,
    FILTER_SSV_POSITION,
    TRAJECTORY_LENGTH,
    GAUSSIAN_COLOR,
    SSV_COLOR,
    DOWNSAMPLE_RESOLUTION,
  };

  ar::Ellipsoid generateEllipsoid(
          const Eigen::Vector3d& mean,
          const Eigen::Matrix3d& covar,
          const ar::Color& color) {

    const double sphereRadius = 2.7955; // 95% probability mass

    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eigensolver(covar);
    const Eigen::Vector3d evals = eigensolver.eigenvalues();
    const Eigen::Matrix3d evecs = eigensolver.eigenvectors() * evals.cwiseSqrt().asDiagonal();

    return ar::Ellipsoid(mean.data(), evecs.data(), sphereRadius, color);
  }

} // namespace anonymous

/**
 *
 */
class ObstacleTrackerVisualizer : public BaseVisualizer, public GMM::GMMDataObserver
{

public:
  enum ColorMode
  {
    NONE,
    SOFT_ASSIGNMENT,
    HARD_ASSIGNMENT,
    NR_ITEMS // this must ALWAYS be the LAST enum value!
  };

  struct GUIParams
  {
    bool drawGaussians;
    bool drawSSVs;
    bool drawTrajectories;
    bool drawVelocities;
    bool drawDebugValues;
    bool drawVoxels;
    uint trajectoryLength;
    bool enableTracker;  // should not be a runtime option
    bool enableTightFit; // should not be a runtime option
    float downsampleResolution;
    ColorMode colorMode;
  };

  struct VisData
  {
    bool isCapsule;
    ar::mesh_handle ssvHandle;
    ar::mesh_handle ellipsoidHandle;
    ar::mesh_handle linePathHandle;
    ar::mesh_handle velocityLineHandle;
    ar::BufferedLinePath* bufferedLinePath;
    double ssvRadius;
    Eigen::Vector3d ssvPointA;
    Eigen::Vector3d ssvPointB;
  };

  ObstacleTrackerVisualizer(GUIParams const& parameters,
                            std::string const& name = "lepp3",
                            int const& width = 1024,
                            int const& height = 768)
      : BaseVisualizer(name, width, height),
        main_cloud_data_(ar::PCL_PointXYZ),
        gmm_cloud_data_(ar::PCL_PointXYZRGBA){

    debug_gui_params_ = parameters;
    main_cloud_handle_ = arvis_->Add(main_cloud_data_);
    gmm_cloud_handle_ = arvis_->Add(gmm_cloud_data_);
    initUI();
  }

  ~ObstacleTrackerVisualizer() { }
  /**
   * FrameDataObserver interface implementation: processes the current point cloud.
   */
  virtual void updateFrame(FrameDataPtr frameData);
  /**
* `RGBDataObserver` interface implementation.
*/
  void updateFrame(RGBDataPtr rgbData);
  /**
   * GMMObstacleTrackerAggregator interface implementation. Process any additional
   * data generated by GMMObstacleDetector
   */
  virtual void updateObstacleTrackingData(
        ar::PointCloudData const& cloud_data,
        lepp::util::VoxelGrid3D const& vg
//-        GMM::RuntimeStat runtime_stats);
        );
  /**
   * GMMObstacleTrackerAggregator interface implementation. Process any additional
   * data generated by GMMObstacleDetector: current obstacle state based on the
   * Kalman filter.
   */
  virtual void updateState(GMM::State& state, size_t idx);
  void deleteState(GMM::State& state, size_t id);
private:
  /**
   *
   */
  void initUI();
  template<typename T> T getUiOption(Ui_Option option) const { }
  /**
   * The following 3 methods are called based on the flag received by the
   * `updateObstacleState` method. Will add additional information/visualization
   * to the visualizer.
   */
  void initVisData(GMM::State& state, size_t id);
  void updateVisData(GMM::State& state, size_t id);

  ar::mesh_handle main_cloud_handle_;
  ar::PointCloudData main_cloud_data_;
  ar::mesh_handle gmm_cloud_handle_;
  ar::PointCloudData gmm_cloud_data_;

  std::map<int, VisData> gmm_visualizations; // GMM state ID mapped to visualization data for each state

  GUIParams debug_gui_params_;
  /// UI elements
  ar::IUIWindow* _windowMain;
  ar::IUIWindow* _windowStats;
  ar::ui_element_handle _statMainAlgorithmTime;
  ar::ui_element_handle _statDeltaT;
  ar::ui_element_handle _checkBoxEnabled;
  ar::ui_element_handle _checkBoxDrawGaussians;
  ar::ui_element_handle _checkBoxDrawSSVs;
  ar::ui_element_handle _checkBoxDrawTrajectories;
  ar::ui_element_handle _checkBoxDrawVelocities;
  ar::ui_element_handle _checkBoxDrawDebugValues;
  ar::ui_element_handle _checkBoxDrawVoxels;
  ar::ui_element_handle _checkBoxEnableTightFit;
  ar::ui_element_handle _checkBoxFilterSSVPositions;
  ar::ui_element_handle _comboBoxColorMode;
  ar::ui_element_handle _dragIntTrajectoryLength;
  ar::ui_element_handle _colorEditGaussians;
  ar::ui_element_handle _colorEditSSVs;
  ar::ui_element_handle _sliderFloatDownsampleResolution;

  ar::ui_element_handle _floatRangeBoundsX;
  ar::ui_element_handle _floatRangeBoundsZ;

  //
  bool _drawVoxels;
};


/// STANDARD: If a template, a member template or a member of a class template
/// is explicitly specialized then that specialization shall be declared before
/// the first use of that specialization that would cause an implicit
/// instantiation to take place, in every translation unit in which such a use
/// occurs; no diagnostic is required.
template <>
bool ObstacleTrackerVisualizer::getUiOption<bool>(Ui_Option option) const
{
  switch (option)
  {
    case Ui_Option::ENABLE_TRACKER:
      return _windowMain->GetCheckBoxState(_checkBoxEnabled);
    case Ui_Option::ENABLE_TIGHT_FIT:
      return _windowMain->GetCheckBoxState(_checkBoxEnableTightFit);
    case Ui_Option::DRAW_GAUSSIAN:
      return _windowMain->GetCheckBoxState(_checkBoxDrawGaussians);
    case Ui_Option::DRAW_SSV:
      return _windowMain->GetCheckBoxState(_checkBoxDrawSSVs);
    case Ui_Option::DRAW_TRAJECTORY:
      return _windowMain->GetCheckBoxState(_checkBoxDrawTrajectories);
    case Ui_Option::DRAW_VELOCIY:
      return _windowMain->GetCheckBoxState(_checkBoxDrawVelocities);
    case Ui_Option::DRAW_DEBUG_VALUE:
      return _windowMain->GetCheckBoxState(_checkBoxDrawDebugValues);
    case Ui_Option::DRAW_VOXEL:
      return _windowMain->GetCheckBoxState(_checkBoxDrawVoxels);
    case Ui_Option::FILTER_SSV_POSITION:
      return _windowMain->GetCheckBoxState(_checkBoxFilterSSVPositions);
    default:
      throw std::exception();
  }
}

template <>
int ObstacleTrackerVisualizer::getUiOption<int>(Ui_Option option) const
{
  switch (option)
  {
    case Ui_Option::TRAJECTORY_LENGTH:
      return _windowMain->GetSliderIntValue(_dragIntTrajectoryLength);
    default:
      throw std::exception();
  }
}

template <>
float ObstacleTrackerVisualizer::getUiOption<float>(Ui_Option option) const
{
  switch (option)
  {
    case Ui_Option::DOWNSAMPLE_RESOLUTION:
      return _windowMain->GetSliderFloatValue(_sliderFloatDownsampleResolution);
    default:
      throw std::exception();
  }
}

template <>
ar::Color ObstacleTrackerVisualizer::getUiOption<ar::Color>(Ui_Option option) const
{
  float color[4];
  switch (option)
  {
    case Ui_Option::GAUSSIAN_COLOR:
      _windowMain->GetColorValues4(_colorEditGaussians, color);
      break;
    case Ui_Option::SSV_COLOR:
      _windowMain->GetColorValues4(_colorEditSSVs, color);
      break;
    default:
      throw std::exception();
  }

  return ar::Color(color[0], color[1], color[2], color[3]);
}
///

void ObstacleTrackerVisualizer::initUI() {
  _windowMain = arvis_->AddUIWindow("Obstacle Tracker");
  _windowStats = arvis_->AddUIWindow("Obstacle Tracker Stats");

  _windowMain->AddText("Visualization:");
  _checkBoxDrawGaussians = _windowMain->AddCheckBox("Draw Gaussians", debug_gui_params_.drawGaussians);
  _checkBoxDrawSSVs = _windowMain->AddCheckBox("Draw SSVs", debug_gui_params_.drawSSVs);
  _checkBoxDrawTrajectories = _windowMain->AddCheckBox("Draw Trajectories", debug_gui_params_.drawTrajectories);
  _checkBoxDrawVelocities = _windowMain->AddCheckBox("Draw Velocities", debug_gui_params_.drawVelocities);
  _checkBoxDrawDebugValues = _windowMain->AddCheckBox("Draw Debug Values", debug_gui_params_.drawDebugValues);
  _checkBoxDrawVoxels = _windowMain->AddCheckBox("Draw Voxels", debug_gui_params_.drawVoxels);
  const char* colorModes[ColorMode::NR_ITEMS] = { "No Color", "Soft Assignment", "Hard Assignment" };
  _comboBoxColorMode = _windowMain->AddComboBox("Color Mode", colorModes, ColorMode::NR_ITEMS, ColorMode::SOFT_ASSIGNMENT);
  _dragIntTrajectoryLength = _windowMain->AddDragInt("Traj. Length", 1, 1000, 0.0f, debug_gui_params_.trajectoryLength);
  float color[4] { 1.0f, 0.35f, 0.2f, 0.7f };
  _colorEditGaussians = _windowMain->AddColorEdit4("Gauss. Color", color);
  _colorEditSSVs = _windowMain->AddColorEdit4("SSV Color", color);
  _windowMain->AddSeparator();
  _windowMain->AddText("Tracker Options:");
  _checkBoxEnabled = _windowMain->AddCheckBox("Enable", debug_gui_params_.enableTracker);
  _checkBoxEnableTightFit = _windowMain->AddCheckBox("Tight Fit", debug_gui_params_.enableTightFit);
  _sliderFloatDownsampleResolution = _windowMain->AddSliderFloat("Downsample Res.", 0.005f, 0.1f, 0.03f);

  // _checkBoxFilterSSVPositions = _windowMain->AddCheckBox("Filter SSV Positions", debug_gui_params_.filter_ssv_positions);

  _statMainAlgorithmTime = _windowStats->AddPlot("Main Algorithm Time", 0.0f, 100.0f, 128, 50.0f);
  _statDeltaT = _windowStats->AddPlot("DeltaT", FLT_MAX, FLT_MAX, 128, 50.0f);

  // if (parameters.enable_crop_cloud_in_ui)
  // {
  //   _windowMain->AddSeparator();
  //   _windowMain->AddText("Crop Region:");
  //   float minX = -0.95f, maxX = 0.77f, minZ = -2.5f, maxZ = 0.0f;
  //   _floatRangeBoundsX = _windowMain->AddFloatRange("BoundsX", 0.01f, 0.0f, 0.0f, minX, maxX);
  //   _floatRangeBoundsZ = _windowMain->AddFloatRange("BoundsZ", 0.01f, 0.0f, 0.0f, minZ, maxZ);
  // }
}

void ObstacleTrackerVisualizer::updateFrame(FrameDataPtr frameData) {
  // Show the original point cloud.
  // TODO: [Sahand] Visualize the obstacles
  // There's a chance that cbuttner modifies this one.
  main_cloud_data_.pointData = reinterpret_cast<const void*>(&(frameData->cloud->points[0]));
  main_cloud_data_.numPoints = frameData->cloud->size();
  arvis_->Update(main_cloud_handle_, main_cloud_data_);
}

void ObstacleTrackerVisualizer::updateFrame(RGBDataPtr rgbData) {
  return;
};

void ObstacleTrackerVisualizer::updateObstacleTrackingData(
      ar::PointCloudData const& cloud_data,
      lepp::util::VoxelGrid3D const& vg
//-      GMM::RuntimeStat runtime_stats) {
      ) {

  // Visualize the result cloud
  gmm_cloud_data_.numPoints = cloud_data.numPoints;
  gmm_cloud_data_.color = cloud_data.color;

  arvis_->Update(gmm_cloud_handle_, gmm_cloud_data_);
  // Visualize the voxel grid (if set in the GUI)
  // draw voxels if enabled, otherwise remove
  if (_windowMain->GetCheckBoxState(_checkBoxDrawVoxels)) {
    Vector<ar::Voxel> ar_voxels;
    vg.prepareArVoxel(ar_voxels);
    arvis_->DrawVoxels(ar_voxels.data(), ar_voxels.size());
  }
  else {
    arvis_->RemoveAllVoxels();
  }
  // Visualize the runtime stats
//-  _windowStats->PushPlotValue(_statMainAlgorithmTime, runtime_stats.MainAlgorithmTime);
//-  _windowStats->PushPlotValue(_statDeltaT, runtime_stats.DeltaT);
}

void ObstacleTrackerVisualizer::updateState(GMM::State& state, size_t idx)
{
  auto visData = gmm_visualizations.find(idx);
  if (visData == gmm_visualizations.end())
  {
    initVisData(state, idx);
  }
  else
  {
    updateVisData(state, idx);
  }
}

void ObstacleTrackerVisualizer::updateVisData(GMM::State& state, size_t id)
{
  auto& visData = gmm_visualizations[id];

//   auto const filteredState = state.kalmanFilter.getState();
//
//   const Eigen::Vector3d pos = filteredState.position().cast<double>();
//   const Eigen::Vector3d vel = filteredState.velocity().cast<double>() + pos;
//   const Eigen::Matrix3d cov = state.obsCovar.cast<double>();
  const Eigen::Vector3d pos = state.pos.cast<double>();
   // Ellipsoid
  const ar::Color color = getUiOption<ar::Color>(Ui_Option::GAUSSIAN_COLOR);
  arvis_->Update(visData.ellipsoidHandle, generateEllipsoid(state.pos.cast<double>(), state.obsCovar.cast<double>(), color));
  // Line path / trajectory
  visData.bufferedLinePath->addPoint(pos.data());
  arvis_->Update(visData.linePathHandle, *visData.bufferedLinePath);
  // Velocity
  //arvis_->Update(visData.velocityLineHandle, ar::LineSegment(state.pos.data(), vel.data(), 0.005f));

  // TODO: [Sahand] Two colors are exactly the same??
  const ar::Color ssvColors[2] { getUiOption<ar::Color>(Ui_Option::SSV_COLOR), getUiOption<ar::Color>(Ui_Option::SSV_COLOR) };
  // SSV
  if (visData.ssvHandle == 0 && visData.ssvRadius > 0.01)
  {
    // add if not added yet
    if (!visData.isCapsule)
      visData.ssvHandle = arvis_->Add(ar::Sphere(visData.ssvPointA.data(), visData.ssvRadius, ssvColors[0]));
    else
      visData.ssvHandle = arvis_->Add(ar::Capsule(visData.ssvPointA.data(), visData.ssvPointB.data(), visData.ssvRadius, ssvColors[1]));
  }
  else if (visData.ssvRadius > 0.01)
  {
    if (!visData.isCapsule)
      arvis_->Update(visData.ssvHandle, ar::Sphere(visData.ssvPointA.data(), visData.ssvRadius, ssvColors[0]));
    else
      arvis_->Update(visData.ssvHandle, ar::Capsule(visData.ssvPointA.data(), visData.ssvPointB.data(), visData.ssvRadius, ssvColors[1]));
  }

  // set object visibilities according to UI settings
  const bool shouldDrawGaussians = getUiOption<bool>(Ui_Option::DRAW_GAUSSIAN);
  const bool shouldDrawSSVs = getUiOption<bool>(Ui_Option::DRAW_SSV);
  const bool shouldDrawTrajectories = getUiOption<bool>(Ui_Option::DRAW_TRAJECTORY);
  const bool shouldDrawVelocities = getUiOption<bool>(Ui_Option::DRAW_VELOCIY);
}

void ObstacleTrackerVisualizer::initVisData(GMM::State& state, size_t id)
{
  VisData visData;

  const Eigen::Vector3d pos = state.pos.cast<double>();
  const Eigen::Matrix3d cov = state.obsCovar.cast<double>();

  // Ellipsoid
  const ar::Color color = getUiOption<ar::Color>(Ui_Option::GAUSSIAN_COLOR);
  visData.ellipsoidHandle = arvis_->Add(generateEllipsoid(pos, cov, color));

  // Line path / trajectory
  visData.bufferedLinePath = new ar::BufferedLinePath(getUiOption<int>(Ui_Option::TRAJECTORY_LENGTH), 0.003f, ar::Color(1,1,1,1));
  visData.linePathHandle = arvis_->Add(*visData.bufferedLinePath);

  // Velocity
  visData.velocityLineHandle = arvis_->Add(ar::LineSegment(pos.data(), pos.data(), 0.005f));

  // SSV (add later)
  visData.ssvHandle = 0;

  gmm_visualizations[id] = visData;
}

void ObstacleTrackerVisualizer::deleteState(GMM::State& state, size_t id)
{
  auto visData = gmm_visualizations.find(id);
  if (visData != gmm_visualizations.end())
  {
    arvis_->Remove(visData->second.ellipsoidHandle);
    arvis_->Remove(visData->second.linePathHandle);
    arvis_->Remove(visData->second.velocityLineHandle);
    arvis_->Remove(visData->second.ssvHandle);
    gmm_visualizations.erase(id);
  }
}

} // namespace lepp

#endif // LEPP3_VISUALIZATION_OBSTACLE_TRACKER_H__
