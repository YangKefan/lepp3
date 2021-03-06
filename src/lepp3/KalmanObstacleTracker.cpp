#include "lepp3/KalmanObstacleTracker.hpp"

namespace lepp {

void KalmanObstacleTracker::update(std::vector<lepp::ObjectModelParams>& obstacles)
{
  static double dt = 1.0/30.0; // time since previous update. Assume 30Hz on first call.
             /// NOTE: Accuracy may depend on what's going on further up the pipeline.
             ///       It may be worth looking into adding this to FrameData so we can
             ///       keep it in sync with the sensor updates.
  if (frameTimer_.running())
  {
    frameTimer_.stop();
    dt = frameTimer_.duration() / 1000.0;
  }
  frameTimer_.start();

  for (auto& obstacle : obstacles)
  {
    auto state = states_.find(obstacle.id);
    if (state == states_.end()) // new obstacle
    {
      Eigen::Vector3f pos = Eigen::Vector3f(obstacle.center.x,
                                            obstacle.center.y,
                                            obstacle.center.z);
      ObstacleFilter::State initialState(pos, Eigen::Vector3f::Zero()); // init w/ current position & zero velocity
      ObstacleKalmanFilter kf;
      kf.init(initialState);
      states_[obstacle.id] = kf;
    }
    else // existing obstacle
    {
      // get obstacle's current position
      Eigen::Vector3f pos = Eigen::Vector3f(obstacle.center.x,
                                            obstacle.center.y,
                                            obstacle.center.z);
      ObstacleFilter::SystemModel sys(noise_position, noise_velocity, dt);
      ObstacleFilter::MeasurementModel mm(noise_measurement);
      state->second.predict(sys);
      auto newState = state->second.update(mm, ObstacleFilter::Measurement(pos));

      obstacle.velocity = newState.velocity();
      obstacle.center = newState.position();
    }
  }
}

void KalmanObstacleTracker::reset(int id)
{
  auto state = states_.find(id);
  if (state != states_.end())
  {
    states_.erase(state);
  }
}

}  // namespace lepp
