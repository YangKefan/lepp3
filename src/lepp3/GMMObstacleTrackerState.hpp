#ifndef LEPP3_OBSTACLETRACKERSTATE_H
#define LEPP3_OBSTACLETRACKERSTATE_H

#include <ARVisualizer.hpp>

#include "lepp3/Typedefs.hpp"
#include "lepp3/GMMObstacleFilter.hpp"

namespace lepp
{

namespace GMM {

  // Simplify communication with the `ObstacleTrackerVisualizer`.
  // Used in GMMObstacleTrackerAggregator's `updateObstacleState` method.
  enum VisModifyFlag {
    INIT = 0,
    UPDATE,
    DE_INIT
  };

  struct ObstacleTrackerParams {
    bool enableTightFit;
    bool filterSSVPositions;
    float voxelGridResolution;

    // sigma squared of uniform gaussian noise (these go into the diagonal of Σ)
    float kalman_SystemNoisePosition = 0.01f;
    float kalman_SystemNoiseVelocity = 0.15f;
    float kalman_MeasurementNoise = 0.10f;
  };

  enum ColorMode {
    NONE = 0,
    SOFT_ASSIGNMENT = 1,
    HARD_ASSIGNMENT = 2,
    NR_ITEMS = 3, // TODO: [Sahand] fix this dirty hack to keep # of modes.
  };

  struct RuntimeStat {
    // ctor
    RuntimeStat()
        : MainAlgorithmTime(0.0),
          DeltaT(0.0) {}
    // member variables
    double MainAlgorithmTime;
    double DeltaT;
  };

  struct DebugGUIParams {
    // ctor
    DebugGUIParams()
        : enableTracker(false),
          enableTightFit(true),
          drawGaussians(false),
          drawSSVs(false),
          drawTrajectories(false),
          drawVelocities(false),
          drawDebugValues(false),
          drawVoxels(false),
          colorMode(ColorMode::SOFT_ASSIGNMENT),
          trajectoryLength(128),
          gaussianColor(ar::Color(1.0f, 0.35f, 0.2f, 0.7f)),
          ssvColor(ar::Color(1.0f, 0.35f, 0.2f, 0.7f)),
          downsampleResolution(0.03f) {}
    // member variables
    bool enableTracker;
    bool enableTightFit;
    bool drawGaussians;
    bool drawSSVs;
    bool drawTrajectories;
    bool drawVelocities;
    bool drawDebugValues;
    bool drawVoxels;
    ColorMode colorMode;
    // bool filterSSVPositions; // This is not necssary here. Already defined.
    int trajectoryLength;
    ar::Color gaussianColor;
    ar::Color ssvColor;
    float downsampleResolution;
  }; // struct debugGUI

  // Data needed for visualizing an obstacle
  struct ObstacleVisualizationData {
    ar::mesh_handle ellipsoidHandle;
    ar::mesh_handle velocityLineHandle; // a line showing direction and magnitude of the estimated velocity

    ar::mesh_handle linePathHandle; // trajectory line handle
    ar::BufferedLinePath* bufferedLinePath; // trajectory buffer

    ar::IUIWindow* infoWindow; // debug info window attached to the obstacles center
    ar::ui_element_handle infoWindowTextHandle;

    // SSV visualization
    // for spheres: center in ssvPointA
    // for capsules: points in ssvPointA and ssvPointB
    ar::mesh_handle ssvHandle;
    Eigen::Vector3d ssvPointA;
    Eigen::Vector3d ssvPointB;
    double ssvRadius = 0.0;
    bool isCapsule; // either sphere or capsule
  };

  // Data for the SSV (sphere or capsule) fit to an obstacle
  struct ObstacleSSVData {
    float ssvRadiusSphere;
    float ssvRadiusCapsule;
    // min point projected onto the largest pricipal axis
    float ssvCapsuleMin;
    // max point projected onto the largest pricipal axis
    float ssvCapsuleMax;
    bool initialized = false;
  };

  // The main state of an obstacle
  // TODO: rename (or namespace)
  struct State {
    State(const Eigen::Vector3f& _pos, const Eigen::Matrix3f& _obsCovar) : pos(_pos)
    {
      setObsCovar(_obsCovar);

      hystSplitVal = 0.25f;

      // initialize the filter
      initKalmanFilter();
    }

    void initKalmanFilter()
    {
      const ObstacleFilter::State initialState(pos, Eigen::Vector3f::Zero()); // initial position, zero velocity
      kalmanFilter.init(initialState);
    }

    Eigen::Vector3f pos; // observation mean
    Eigen::Matrix3f obsCovar; // 3x3 observation covariance (don't set directly, use setObsCovar!)

    // cached values for pdf calculation
    Eigen::Matrix3f obsCovarInv; // 3x3 inverse observation covariance
    float logpdfConstantSummand; // cached constant for MVN distributed logpdf: -log((2 * pi)^(3/2) * sqrt(det(obsCovar))
    bool validObsCovar; // false if obsCovar is not invertible

    float pi = 0.0f; // GMM cluster mixing coefficient

    int lifeTime = 0; // frame counter
    int splitCounter = 0; // number of consecutive frames the split condition was met
    int resetNonSplitCounter = 0; // number of time this state was "reset" without splitting


    float debugValue = 0.0f; // can hold any value to be used for debug visualization

    float hystSplitVal; // TODO: rename, comment


    ObstacleKalmanFilter kalmanFilter; // associated kalman filter

    ObstacleSSVData ssvData;
    ObstacleVisualizationData visData;

    // Set the observation covariance and compute cached inverse+determinant and logpdfConstantSummand
    void setObsCovar(const Eigen::Matrix3f& cov)
    {
      obsCovar = cov;

      double det;
      Eigen::Matrix3d inverse;
      cov.cast<double>().computeInverseAndDetWithCheck(inverse, det, validObsCovar);

      if (validObsCovar)
      {
        obsCovarInv = inverse.cast<float>();
        logpdfConstantSummand = static_cast<float>(-log(15.749760995 * sqrt(det))); // 15.749760995 = (2 * pi)^(3/2)
      }
    }
  };
} // namespace GMM

} // namespace lepp

#endif // LEPP3_OBSTACLETRACKERSTATE_H
