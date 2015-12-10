#ifndef LEPP3_FILTERED_VIDEO_SOURCE_H__
#define LEPP3_FILTERED_VIDEO_SOURCE_H__

#include "lepp3/BaseVideoSource.hpp"
#include "lepp3/VideoObserver.hpp"
#include "lepp3/filter/PointFilter.hpp"

#include <algorithm>
#include <numeric>

#include <boost/enable_shared_from_this.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include <boost/circular_buffer.hpp>

#include "lepp3/debug/timer.hpp"

#include "deps/easylogging++.h"

/**
 * A struct that is used to describe a single point in space that can be used
 * to index sets and maps of such points.
 */
struct MapPoint {
  int x;
  int y;
  int z;
  MapPoint(int x, int y, int z) : x(x), y(y), z(z) {}
  MapPoint() {}
};

/**
 * Structs that are to be placed in associative containers must be comparable.
 */
bool operator==(MapPoint const& lhs, MapPoint const& rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

/**
 * Provides a hash value for a `MapPoint` in order to allow for it to be
 * placed in boost's map and set.
 */
size_t hash_value(MapPoint const& pt) {
  std::size_t seed = 0;
  boost::hash_combine(seed, pt.x);
  boost::hash_combine(seed, pt.y);
  boost::hash_combine(seed, pt.z);
  return seed;
}

/**
 * ABC.
 * A VideoSource decorator.  It wraps a given VideoSource instance and emits
 * clouds that are filtered versions of the original raw cloud returned by the
 * wrapped `VideoSource`.
 *
 * The cloud it receives is first filtered by applying a number of point-wise
 * filters to each point. The filters that are applied to points (if any) are
 * set dynamically (`setFilter` method).
 *
 * Then, it delegates to a concrete implementation which needs to handle the
 * cloud-level filtering.
 *
 * Concrete implementations need to provide implementations for three protected
 * hook methods that handle the full-cloud filtering at various stages: before
 * the original cloud's points are transformed, after a particular point is
 * transformed, and finally after all points have been transformed.
 *
 * This ABC provides the plumbing required to turn the filtered source into a
 * `VideoSource` making it simple to implement a concrete filter by simply
 * providing implementations for the mentioned hooks.
 */
template<class PointT>
class FilteredVideoSource
    : public VideoSource<PointT>,
      public VideoObserver<PointT>,
      public boost::enable_shared_from_this<FilteredVideoSource<PointT> > {
public:
  using typename VideoSource<PointT>::PointCloudType;
  /**
   * Creates a new FilteredVideoSource which will perform filtering of each
   * cloud generated by the given source and emit such a filtered cloud to its
   * own observers.
   *
   * The FilteredVideoSource instance does not assume ownership of the given
   * source, but shares it.
   */
  FilteredVideoSource(boost::shared_ptr<VideoSource<PointT> > source)
      : source_(source) {}
  /**
   * Implementation of the VideoSource interface.
   */
  virtual void open();
  /**
   * Implementation of the VideoObserver interface.
   */
  virtual void notifyNewFrame(
      int idx,
      const typename pcl::PointCloud<PointT>::ConstPtr& cloud);

  /**
   * Add a filter that will be applied to individual points before the entire
   * cloud itself is filtered.
   */
  void addFilter(boost::shared_ptr<PointFilter<PointT> > filter) {
    point_filters_.push_back(filter);
  }

protected:
  /**
   * A hook method that concrete implementations of the `FilteredVideoSource`
   * should implement in order to prepare for a new frame's filtering.
   */
  virtual void newFrame() = 0;
  /**
   * A hook method that concrete implementations of the `FilteredVideoSource`
   * should implement to handle new points received in the latest frame.
   *
   * It receives a reference to the cloud that will contain the final filtered
   * representation already, if the implementation wishes to examine or modify
   * it.
   */
  virtual void newPoint(PointT& p, PointCloudType& filtered) = 0;
  /**
   * A hook method that concrete implementations of the `FilteredVideoSource`
   * should implement to return the filtered cloud assembled from the points
   * given by the `newPoint` calls since the previous `newFrame` call.
   * Implementations are free to take into account any historic data and throw
   * away or otherwise modify the points given in `newPoint` calls, though.
   *
   * The filtered cloud should be put into the cloud instance passed as the
   * parameter.
   */
  virtual void getFiltered(PointCloudType& filtered) = 0;

private:
  /**
   * The VideoSource instance that will be filtered by this instance.
   */
  boost::shared_ptr<VideoSource<PointT> > source_;
  /**
   * The filters that are applied to individual points (in the order found in
   * the vector) before passing it off to the concrete cloud filter
   * implementation.
   */
  std::vector<boost::shared_ptr<PointFilter<PointT> > > point_filters_;
};

template<class PointT>
void FilteredVideoSource<PointT>::open() {
  // Start the wrapped VideoSource and make sure that this instance is notified
  // when it emits any new clouds.
  source_->attachObserver(this->shared_from_this());
  source_->open();
}

template<class PointT>
void FilteredVideoSource<PointT>::notifyNewFrame(
    int idx,
    const typename pcl::PointCloud<PointT>::ConstPtr& cloud) {
  Timer t;
  t.start();

  // Prepare the point-wise filters for a new frame.
  {
    size_t sz = point_filters_.size();
    for (size_t i = 0; i < sz; ++i) {
      point_filters_[i]->prepareNext();
    }
  }
  // Prepare the concrete cloud filter implementation for a new frame.
  this->newFrame();

  // Process the cloud received from the wrapped instance and put the filtered
  // result in a new point cloud.
  typename PointCloudType::Ptr cloud_filtered(new PointCloudType());
  PointCloudType& filtered = *cloud_filtered;
  cloud_filtered->is_dense = true;
  cloud_filtered->sensor_origin_ = cloud->sensor_origin_;

  // Apply point-wise filters to each received point and then pass it to the
  // concrete implementation to figure out how to filter the entire cloud.
  for (typename pcl::PointCloud<PointT>::const_iterator it = cloud->begin();
        it != cloud->end();
        ++it) {
    PointT p = *it;
    // Filter out NaN points already, since we're already iterating through the
    // cloud.
    if (!pcl_isfinite(p.x) || !pcl_isfinite(p.y) || !pcl_isfinite(p.z)) {
      continue;
    }

    // Now apply point-wise filters.
    size_t const sz = point_filters_.size();
    bool valid = true;
    for (size_t i = 0; i < sz; ++i) {
      if (!point_filters_[i]->apply(p)) {
        valid = false;
        break;
      }
    }
    // And pass such a filtered/modified point to the cloud-level filter.
    if (valid) this->newPoint(p, filtered);
  }

  // Now we obtain the fully filtered cloud...
  this->getFiltered(filtered);
  // ...and we're done!
  t.stop();

  //LTRACE << "Total included points " << cloud_filtered->size();
  //PINFO << "Filtering took " << t.duration();
  // Finally, the cloud that is emitted by this instance is the filtered cloud.
  this->setNextFrame(cloud_filtered);
}

/**
 * An implementation of a `FilteredVideoSource` that only applies the
 * point-wise filters, without performing any additional cloud-level filtering.
 */
template<class PointT>
class SimpleFilteredVideoSource : public FilteredVideoSource<PointT> {
public:
  using typename FilteredVideoSource<PointT>::PointCloudType;

  SimpleFilteredVideoSource(boost::shared_ptr<VideoSource<PointT> > source)
      : FilteredVideoSource<PointT>(source) {}
protected:
  void newFrame() {}
  void newPoint(PointT& p, PointCloudType& filtered) { filtered.push_back(p); }
  void getFiltered(PointCloudType& filtered) {}
};

/**
 * An implementation of a `FilteredVideoSource` where points are included only
 * if they have been seen in a certain percentage of frames of the last N
 * frames.
 */
template<class PointT>
class ProbFilteredVideoSource : public FilteredVideoSource<PointT> {
public:
  using typename FilteredVideoSource<PointT>::PointCloudType;

  ProbFilteredVideoSource(boost::shared_ptr<VideoSource<PointT> > source)
      : FilteredVideoSource<PointT>(source),
        larger_voxelization_(false) {}
  ProbFilteredVideoSource(boost::shared_ptr<VideoSource<PointT> > source,
                          bool larger_voxelization)
      : FilteredVideoSource<PointT>(source),
        larger_voxelization_(larger_voxelization) {}
protected:
  void newFrame() {
    this_frame_.clear();
    min_pt.x = std::numeric_limits<int>::max();
    min_pt.y = std::numeric_limits<int>::max();
    min_pt.z = std::numeric_limits<int>::max();
    max_pt.x = std::numeric_limits<int>::min();
    max_pt.y = std::numeric_limits<int>::min();
    max_pt.z = std::numeric_limits<int>::min();
  }

  void newPoint(PointT& p, PointCloudType& filtered) {
    MapPoint map_point = MapPoint(p.x * 100, p.y * 100, p.z * 100);

    if (larger_voxelization_) {
      int const mask = 0xFFFFFFFE;
      map_point.x &= mask;
      map_point.y &= mask;
      map_point.z &= mask;
    }

    uint32_t& ref = all_points_[map_point];
    ref <<= 1;
    ref |= 1;
    this_frame_.insert(map_point);
    // Keep track of the min/max points so that we know the bounding box of
    // the current cloud.
    min_pt.x = std::min(min_pt.x, map_point.x);
    min_pt.y = std::min(min_pt.y, map_point.y);
    min_pt.z = std::min(min_pt.z, map_point.z);
    max_pt.x = std::max(max_pt.x, map_point.x);
    max_pt.y = std::max(max_pt.y, map_point.y);
    max_pt.z = std::max(max_pt.z, map_point.z);
  }

  void getFiltered(PointCloudType& filtered) {
    boost::unordered_map<MapPoint, uint32_t>::iterator it =
        all_points_.begin();
    while (it != all_points_.end()) {
      if (this_frame_.find(it->first) == this_frame_.end()) {
        it->second <<= 1;
      }
      int count = __builtin_popcount(it->second);
      if (count >= 10) {
        PointT pt;
        pt.x = it->first.x / 100.;
        pt.y = it->first.y / 100.;
        pt.z = it->first.z / 100.;
        filtered.push_back(pt);
      }

      // Allow for a bit of leeway with removing points at the very boundary of
      // the bounding box. To do this, the bounding box is increased by 10 [cm]
      // in each direction. Only points within *this* bounding box are kept in
      // the map -- all others removed.
      // TODO See if tweaking these values up/down yields any benefits.
      min_pt.x -= 10;
      min_pt.y -= 10;
      min_pt.z -= 10;
      max_pt.x += 10;
      max_pt.y += 10;
      max_pt.z += 10;
      MapPoint const& pt = it->first;
      if (pt.x < min_pt.x || pt.x > max_pt.x ||
          pt.y < min_pt.y || pt.y > max_pt.y ||
          pt.z < min_pt.z || pt.z > max_pt.z) {
        all_points_.erase(it++);
      } else {
        ++it;
      }
    }
  }
private:
  boost::unordered_set<MapPoint> this_frame_;
  boost::unordered_map<MapPoint, uint32_t> all_points_;
  MapPoint min_pt;
  MapPoint max_pt;

  bool const larger_voxelization_;
};

/**
 * An implementation of a `FilteredVideoSource` that applies a PT1 filter on the
 * stream of clouds.
 */
template<class PointT>
class Pt1FilteredVideoSource : public FilteredVideoSource<PointT> {
public:
  using typename FilteredVideoSource<PointT>::PointCloudType;

  Pt1FilteredVideoSource(boost::shared_ptr<VideoSource<PointT> > source)
      : FilteredVideoSource<PointT>(source) {}
protected:
  void newFrame() { this_frame_.clear(); }

  void newPoint(PointT& p, PointCloudType& filtered) {
    MapPoint map_point = MapPoint(p.x * 100, p.y * 100, p.z * 100);
    float& ref = all_points_[map_point];
    // TODO Factor out the parameter value to a member constant.
    ref = 0.9*ref + 0.1*10;
    this_frame_.insert(map_point);
  }

  void getFiltered(PointCloudType& filtered) {
    boost::unordered_map<MapPoint, float>::iterator it =
        all_points_.begin();
    while (it != all_points_.end()) {
      if (this_frame_.find(it->first) == this_frame_.end()) {
        it->second = 0.9*it->second + 0.1*0.;
      }
      if (it->second >= 4.) {
        PointT pt;
        pt.x = it->first.x / 100.;
        pt.y = it->first.y / 100.;
        pt.z = it->first.z / 100.;
        filtered.push_back(pt);
      }
      ++it;
    }
  }
private:
  boost::unordered_set<MapPoint> this_frame_;
  boost::unordered_map<MapPoint, float> all_points_;
};
#endif
