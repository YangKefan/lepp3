#ifndef LEPP3_VISUALIZATION_CALIBRATOR_VISUALIZER_H__
#define LEPP3_VISUALIZATION_CALIBRATOR_VISUALIZER_H__

#include <sstream>

#include "lepp3/Typedefs.hpp"
#include "lepp3/visualization/BaseVisualizer.hpp"
#include "lepp3/models/ObjectModel.h"
#include "lepp3/CalibrationAggregator.hpp"


namespace lepp {

template<class PointT>
class CalibratorVisualizer
  : public BaseVisualizer,
    public CalibrationAggregator<PointT> {

public:
  CalibratorVisualizer(
    std::string const& name = "lepp3", int const& width = 1024, int const& height = 768)
    : BaseVisualizer(name, width, height),
      main_cloud_data_(ar::PCL_PointXYZ),
      largest_plane_data_(ar::PCL_PointXYZRGBA),
      gridData(gridVector, gridSize, gridThickness, ar::Color( 0.5, 0.5, 0.5, 0.5 )),
      cosyX(cosy_o, cosy_x, 0.01f, ar::Color( 1.0, 0.0, 0.0 )),
      cosyY(cosy_o, cosy_y, 0.01f, ar::Color( 0.0, 1.0, 0.0 )),
      cosyZ(cosy_o, cosy_z, 0.01f, ar::Color( 0.0, 0.0, 1.0 )){

  main_cloud_handle_ = arvis_->Add(main_cloud_data_);
  largest_plane_handle_ = arvis_->Add(largest_plane_data_);
  arvis_->Add(cosyX);
  arvis_->Add(cosyY);
  arvis_->Add(cosyZ);
  gridHandle = arvis_->Add(gridData);
  gridWindow = arvis_->AddUIWindow("Grid");
  gridCheckBox = gridWindow->AddCheckBox("Draw", true);

  // Set up the values windows
  ui_values_window_ = arvis_->AddUIWindow("Values", 200.0f, 100.0f);
  mean_z_txt = ui_values_window_->AddText("");
  var_z_txt  = ui_values_window_->AddText("");
  }

  ~CalibratorVisualizer() {}

  /**
   * FrameDataObserver interface implementation: processes the current point cloud.
   */
   virtual void updateFrame(FrameDataPtr frameData);
  /**
   * CalibrationAggregator interface implementation.
   */
  virtual void updateCalibrationParams(
    typename pcl::PointCloud<PointT>::Ptr const& largest_plane,
    const float& mean_z,
    const float& var_z);

private:
  void updateMeanVar(float const& mean_z, float const& var_z);
  void drawLargestPlane(PointCloudPtr const& cloud);
  // Pointcloud data and handles required by the ARVisualizer object
  ar::mesh_handle main_cloud_handle_;
  ar::PointCloudData main_cloud_data_;
  ar::mesh_handle largest_plane_handle_;
  ar::PointCloudData largest_plane_data_;
  // UI elements
  ar::IUIWindow* ui_values_window_;
  ar::ui_element_handle mean_z_txt;
  ar::ui_element_handle var_z_txt;

  //  Coordinate System xyz = rgb, size 0,2m x 0,01m
  double cosy_o[3] = { 0.0, 0.0, 0.0 };
  double cosy_x[3] = { 0.2, 0.0, 0.0 };
  double cosy_y[3] = { 0.0, 0.2, 0.0 };
  double cosy_z[3] = { 0.0, 0.0, 0.2 };
  ar::LineSegment cosyX;
  ar::LineSegment cosyY;
  ar::LineSegment cosyZ;

//  Grid for visualization reference: 4m x 4m, with corners at
//  (0,-2, 0)
//  (0, 2, 0)
//  (4,-2, 0)
//  (4, 2, 0)
  ar::mesh_handle gridHandle;
  float gridThickness = 0.001f;
  size_t gridSize = 19;
  double gridVector[57] = {
          0.0,-2.0, 0.0,
          0.0, 2.0, 0.0,
          1.0, 2.0, 0.0,
          1.0,-2.0, 0.0,
          2.0,-2.0, 0.0,
          2.0, 2.0, 0.0,
          3.0, 2.0, 0.0,
          3.0,-2.0, 0.0,
          4.0,-2.0, 0.0,
          4.0, 2.0, 0.0,
          0.0, 2.0, 0.0,
          0.0, 1.0, 0.0,
          4.0, 1.0, 0.0,
          4.0, 0.0, 0.0,
          0.0, 0.0, 0.0,
          0.0,-1.0, 0.0,
          4.0,-1.0, 0.0,
          4.0,-2.0, 0.0,
          0.0,-2.0, 0.0
  };
  ar::LinePath gridData;
  ar::IUIWindow* gridWindow;
  ar::ui_element_handle gridCheckBox;
};

template<class PointT>
void CalibratorVisualizer<PointT>::updateMeanVar(float const& mean_z, float const& var_z) {
  std::stringstream m;
  m << "Mean_Z: " << mean_z;

  std::stringstream v;
  v << "Var_Z : " << var_z;

  std::string const mean = m.str();
  std::string const var = v.str();
  ui_values_window_->UpdateText(mean_z_txt, "%s", mean.c_str());
  ui_values_window_->UpdateText(var_z_txt, "%s", var.c_str());
}

template<class PointT>
void CalibratorVisualizer<PointT>::drawLargestPlane(PointCloudPtr const& plane) {
  // Colorize the pointcloud based on the Z value
  typename pcl::PointCloud<pcl::PointXYZRGBA>::Ptr color_cloud(
      new pcl::PointCloud<pcl::PointXYZRGBA>);
  size_t sz = plane->points.size();
  for(size_t i=0; i<sz; ++i) {
    pcl::PointXYZ p = plane->points[i];
    pcl::PointXYZRGBA p_c;
    p_c.x = p.x;
    p_c.y = p.y;
    p_c.z = p.z;
    if (p.z > 0) {
      unsigned char r = 0;
      unsigned char g = 255;
      unsigned char b = 0;
      p_c.r = r;
      p_c.g = g;
      p_c.b = b;
    } else {
      unsigned char r = 255;
      unsigned char g = 0;
      unsigned char b = 0;
      p_c.r = r;
      p_c.g = g;
      p_c.b = b;
    }
    color_cloud->points.push_back(p_c);
  }
  // Add the colored point cloud to the visualizer
  largest_plane_data_.pointData = reinterpret_cast<const void*>(&(color_cloud->points[0]));
  largest_plane_data_.numPoints = color_cloud->size();
  arvis_->Update(largest_plane_handle_, largest_plane_data_);
}

template<class PointT>
void CalibratorVisualizer<PointT>::updateFrame(FrameDataPtr frameData) {
  // visualize the point cloud
  // TODO: Decide whether to show the full point cloud in this visualizer
  main_cloud_data_.pointData = reinterpret_cast<const void*>(&(frameData->cloud->points[0]));
  main_cloud_data_.numPoints = frameData->cloud->size();
  arvis_->Update(main_cloud_handle_, main_cloud_data_);
  arvis_->SetVisibility(gridHandle, (bool)gridWindow->GetCheckBoxState(gridCheckBox));
}

template<class PointT>
void CalibratorVisualizer<PointT>::updateCalibrationParams(
    typename pcl::PointCloud<PointT>::Ptr const& largest_plane,
    const float& mean_z,
    const float& var_z) {

  // Show the new Mean and variance values on the visualizer.
  updateMeanVar(mean_z, var_z);
  // Draw the largest plane found on the scene.
  drawLargestPlane(largest_plane);
}

} // namespace lepp

#endif // LEPP3_VISUALIZATION_CALIBRATOR_VISUALIZER_H__
