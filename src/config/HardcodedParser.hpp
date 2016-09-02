#ifndef LEPP3_CONFIG_HARDCODED_PARSER_H_
#define LEPP3_CONFIG_HARDCODED_PARSER_H_

#include "Parser.h"

/**
 * An implementation of the `Parser` base class.
 *
 * It provides a hardcoded pipeline configuration, with only a relatively small
 * number of parameters that are configurable by passing command line options.
 *
 * The CLI arguments need to be passed to the `HardcodedParser` at
 * construct-time.
 */
template<class PointT>
class HardcodedParser : public Parser<PointT> {
public:
  /**
   * Creates a new `HardcodedParser` based on the given CLI arguments.
   */
  HardcodedParser(char* argv[], int argc) : argv(argv), argc(argc) {
    live_ = checkLive();
    // Perform the initialization in terms of the provided template init.
    this->init();
  }

  /**
   * Returns whether the run is within a live-Parser.
   */
  bool isLive() { return live_; }
protected:
  /// Implementations of initialization of various parts of the pipeline.
  virtual void initRawSource() override {
    this->raw_source_.reset(GetVideoSource());
    if (!this->raw_source_) {
      throw "Unable to initialize the video source";
    }
  }

  virtual void initFilteredVideoSource() override {
    this->filtered_source_.reset(
        new SimpleFilteredVideoSource<PointT>(this->raw_source_));
  }

  virtual void addFilters() override {
    {
      double const a = 1.0117;
      double const b = -0.0100851;
      boost::shared_ptr<PointFilter<PointT> > filter(
          new SensorCalibrationFilter<PointT>(a, b));
      this->filtered_source_->addFilter(filter);
    }
    if (isLive()) {
      boost::shared_ptr<PointFilter<PointT> > filter(
          new RobotOdoTransformer<PointT>(this->pose_service_));
      this->filtered_source_->addFilter(filter);
    }
    if (isLive()) {
      double const xmax = 4;
      double const xmin = -1;
      double const ymax = 1.5;
      double const ymin = -1.5;
      boost::shared_ptr<PointFilter<PointT> > filter(
          new CropFilter<PointT>(xmax,xmin,ymax,ymin));
      this->filtered_source_->addFilter(filter);
    }
    {
      boost::shared_ptr<PointFilter<PointT> > filter(
          new TruncateFilter<PointT>(2));
      this->filtered_source_->addFilter(filter);
    }
  }

  virtual void initPoseService() override {
    this->pose_service_.reset(new PoseService("127.0.0.1", 5000));
    this->pose_service_->start();
  }

  virtual void initVisionService() override {
    boost::shared_ptr<AsyncRobotService> async_robot_service(
        new AsyncRobotService("127.0.0.1", 1337, 10));
    async_robot_service->start();
    this->robot_service_ = async_robot_service;
  }

  virtual void initSurfObstDetector() override
  {
    // Prepare the approximator that the detector is to use.
    // First, the simple approximator...
    boost::shared_ptr<ObjectApproximator<PointT> > simple_approx(
        this->getApproximator());
    // ...then the split strategy
    boost::shared_ptr<SplitStrategy<PointT> > splitter(
        this->buildSplitStrategy());
    // ...finally, wrap those into a `SplitObjectApproximator` that is given
    // to the detector.
    boost::shared_ptr<ObjectApproximator<PointT> > approx(
        new SplitObjectApproximator<PointT>(simple_approx, splitter));
    // Prepare the base detector...
    base_obstacle_detector_.reset(new ObstacleDetector<PointT>(approx,false));

    this->source()->FrameDataSubject::attachObserver(base_obstacle_detector_);
    // Smooth out the basic detector by applying a smooth detector to it
    boost::shared_ptr<SmoothObstacleAggregator> smooth_detector(
        new SmoothObstacleAggregator);
    base_obstacle_detector_->attachObserver(smooth_detector);
    // Now the detector that is exposed via the context is a smoothed-out
    // base detector.
    this->detector_ = smooth_detector;
  }

  virtual void initRecorder() override {}
  virtual void initCamCalibrator() override {}

  virtual void addAggregators() override {
    boost::shared_ptr<LolaAggregator> lola_viewer(
        new LolaAggregator("127.0.0.1", 53250));
    this->detector_->attachObserver(lola_viewer);

    boost::shared_ptr<RobotAggregator> robot_aggregator(
        new RobotAggregator(*this->robot_service(), 30, *this->robot()));
    this->detector_->attachObserver(robot_aggregator);
  }

  virtual void initVisualizers() override {
    // Factor out to a member ...
    bool visualization = true;
    if (visualization) {
      this->visualizers_.reset(new ARVisualizer(false, false));
      // Attach the visualizer to both the point cloud source...
      this->source()->FrameDataSubject::attachObserver(this->visualizers_);
      // ...as well as to the obstacle detector
      this->detector_->attachObserver(this->visualizers_);
    }
  }

private:
  /// Private helper member functions
  /**
   * Checks whether the CLI parameters indicate that the run should be "live",
   * i.e. whether the communication with the robot should be enabled.
   */
  bool checkLive() {
    for (int i = 0; i < argc; ++i) {
      if (std::string(argv[i]) == "--live") return true;
    }
    return false;
  }

  /**
   * Gets a `VideoSource` instance that corresponds to the CLI parameters.
   */
  VideoSource<PointT>* GetVideoSource() const {
    if (argc < 2) {
      return nullptr;
    }

    std::string const option = argv[1];
    if (option == "--stream") {
      return new LiveStreamSource<PointT>();
    } else if (option == "--pcd" && argc >= 3) {
      std::string const file_path = argv[2];
      boost::shared_ptr<pcl::Grabber> interface(new pcl::PCDGrabber<PointT>(
            file_path,
            20.,
            true));
      return new GeneralGrabberVideoSource<PointT>(interface);
    } else if (option == "--oni" && argc >= 3) {
      std::string const file_path = argv[2];
      boost::shared_ptr<pcl::Grabber> interface(new pcl::io::OpenNI2Grabber(
            file_path,
            pcl::io::OpenNI2Grabber::OpenNI_Default_Mode,
            pcl::io::OpenNI2Grabber::OpenNI_Default_Mode));
      return new GeneralGrabberVideoSource<PointT>(interface);
    }

    // Unknown option: return a nullptr.
    return nullptr;
  }

  /// Private member variables

  // The CLI arguments
  char** const argv;
  int const argc;

  /**
   * Whether the run is "live".
   */
  bool live_;

  /**
   * The base detector that we attach to the video source and to which, in
   * turn, the "smooth" detector is attached. The `Parser` maintains a
   * reference to it to make sure it doesn't get destroyed, although it is
   * never exposed to any outside clients.
   */
  boost::shared_ptr<ObstacleDetector<PointT> > base_obstacle_detector_;
};

#endif
