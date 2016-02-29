#ifndef VIDEO_OBSERVER_H_
#define VIDEO_OBSERVER_H_

#include "lepp3/Typedefs.hpp"
#include "lepp3/FrameData.hpp"

namespace lepp {

/**
 * An ABC (interface) that classes that wish to observe a particular
 * VideoSource need to implement.
 */
template<class PointT>
class VideoObserver {
public:
  /**
   * Method that the observers need to implement in order to handle a new
   * incoming frame.  Each frame is represented by its an integer identifier
   * (representing the number of the frame in the sequence of the source)
   * and the point cloud extracted from that frame.
   */
  virtual void notifyNewFrame(
      boost::shared_ptr<FrameData> frameData) = 0;
};

}  // namespace lepp

#endif
