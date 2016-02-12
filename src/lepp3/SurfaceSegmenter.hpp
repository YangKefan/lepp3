#ifndef lepp3_SURFACE_SEGMENTER_H__
#define lepp3_SURFACE_SEGMENTER_H__

#include "lepp3/Typedefs.hpp"
#include "lepp3/BaseSegmenter.hpp"

#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/voxel_grid.h>

namespace lepp {

/**
 * TODO put comments
 */
template<class PointT>
class SurfaceSegmenter: public BaseSegmenter<PointT> {
public:
    SurfaceSegmenter();

	virtual void segment(const PointCloudConstPtr& cloud,
			std::vector<PointCloudConstPtr> &surfaces,
			PointCloudPtr &cloudMinusSurfaces,
			std::vector<pcl::ModelCoefficients> *&m_coefficients);
private:
	// Private helper member functions
	/**
	 * Performs some initial preprocessing and filtering appropriate for the
	 * segmentation algorithm.
	 * Takes the original cloud as a parameter and returns a pointer to a newly
	 * created (and allocated) cloud containing the result of the filtering.
	 */
	PointCloudPtr preprocessCloud(PointCloudConstPtr const& cloud);
	/**
	 * Removes all planes from the given point cloud.
	 */
    void findSurfaces(PointCloudPtr const& cloud_filtered);
	/**
	 * Extracts the Euclidean clusters from the given point cloud.
	 * Returns a vector where each element represents the pcl::PointIndices
	 * instance representing the corresponding cluster.
	 */
    std::vector<pcl::PointIndices> getSurfaceClusters(PointCloudPtr const& cloud);
	/**
	 * Convert the clusters represented by the given indices to point clouds,
	 * by copying the corresponding points from the cloud to the corresponding
	 * new point cloud.
	 */
	void clustersToPointClouds(int cloudIndex,
			std::vector<pcl::PointIndices> const& cluster_indices);

	/* Classification function between segmentation and clustering steps.
	 * Classify any segmented planar plane into one of the segmented surface group,
	 * regarding its plane normal.
	 * This function is necessary to be able to separate ramps or any inclined
	 * surface from the floor at the clustering step.
	 * */

	void classify(PointCloudPtr const& cloud_planar_surface,
			const pcl::ModelCoefficients & coeffs);

	/*Returns the angle between two plane normals.
	 * Calculation is made based on plane coefficients
	 */
	double getAngle(const pcl::ModelCoefficients &coeffs, const int &index);

	/*This function is called to add the segmented plane to a corresponding
	 *surface group. Surface planes normals devising 3 degrees are considered same
	 *type of surface and saved together.
	 **/
	void cluster();

	/**
	 * Instance used to extract the planes from the input cloud.
	 */
	pcl::SACSegmentation<PointT> segmentation_;
	/**
	 * Instance used to extract the actual clusters from the input cloud.
	 */
	pcl::EuclideanClusterExtraction<PointT> clusterizer_;
	/**
	 * The KdTree will hold the representation of the point cloud which is passed
	 * to the clusterizer.
	 */

	/**
     * The cloud that holds all planar surfaces (surfaces).
	 */
    PointCloudPtr cloud_surfaces_;
	/**
	 * The percentage of the original cloud that should be kept for the
	 * clusterization, at the least.
	 * We stop removing planes from the original cloud once there are either no
	 * more planes to be removed or when the number of points remaining in the
	 * cloud dips below this percentage of the original cloud.
	 */

	/**
     * TEMP: vector containing clouds for each surface
	 */
    std::vector<PointCloudConstPtr> vec_cloud_surfaces_;

	/* The vector containing surface groups, created according to difference in inclination during the segmentation
	 * */
	std::vector<PointCloudPtr> vec_surface;


	std::vector<PointCloudConstPtr> vec_segments;


	/* List of plane coefficients [normal_x normal_y normal_z hessian_component_d]
	 *
	 */
	std::vector<pcl::ModelCoefficients> m_coefficients;

	/**
	* List of plane coefficients for 'final' (clustered) surfaces.
	*/
	//std::vector<pcl::ModelCoefficients> surfaceCoefficients;

	/*Segmentation ratio*/
	double const min_filter_percentage_;
};

template<class PointT>
SurfaceSegmenter<PointT>::SurfaceSegmenter() :
        min_filter_percentage_(0.1), cloud_surfaces_(new PointCloudT()) {
	// Parameter initialization of the plane segmentation
	segmentation_.setOptimizeCoefficients(true);
	segmentation_.setModelType(pcl::SACMODEL_PLANE);
	segmentation_.setMethodType(pcl::SAC_RANSAC);
	segmentation_.setMaxIterations(200); // value recognized by Irem
	segmentation_.setDistanceThreshold(0.02);

	// Parameter initialization of the clusterizer

}

template<class PointT>
PointCloudPtr SurfaceSegmenter<PointT>::preprocessCloud(
		PointCloudConstPtr const& cloud) {
	// Remove NaN points from the input cloud.
	// The pcl API forces us to pass in a reference to the vector, even if we have
	// no use of it later on ourselves.
	PointCloudPtr cloud_filtered(new PointCloudT());
	std::vector<int> index;
	pcl::removeNaNFromPointCloud<PointT>(*cloud, *cloud_filtered, index);

	return cloud_filtered;
}

template<class PointT>
void SurfaceSegmenter<PointT>::findSurfaces(PointCloudPtr const& cloud_filtered) {

    vec_cloud_surfaces_.clear();
	m_coefficients.clear();
	vec_surface.clear();
    cloud_surfaces_->clear();
	vec_segments.clear();
	//surfaceCoefficients.clear();

	// Instance that will be used to perform the elimination of unwanted points
	// from the point cloud.
	pcl::ExtractIndices<PointT> extract;
	// Will hold the indices of the next extracted plane within the loop
	pcl::PointIndices::Ptr current_plane_indices(new pcl::PointIndices);
	// Another instance of when the pcl API requires a parameter that we have no
	// further use for.
	// Remove planes until we reach x % of the original number of points
	size_t const original_cloud_size = cloud_filtered->size();
	size_t const point_threshold = min_filter_percentage_ * original_cloud_size;

	bool first=true;
	while (cloud_filtered->size() > point_threshold) {
		// Try to obtain the next plane...
		pcl::ModelCoefficients coefficients;
		segmentation_.setInputCloud(cloud_filtered);
		segmentation_.segment(*current_plane_indices, coefficients);

		// We didn't get any plane in this run. Therefore, there are no more planes
		// to be removed from the cloud.
		if (current_plane_indices->indices.size() == 0) {
			//std::cout << "cannot find more planes > BREAK" << std::endl;
			break;
		}

		// Cloud that holds a plane in each iteration, to be added to the total cloud.
		PointCloudPtr cloud_planar_surface(new PointCloudT());

        // Add the planar inliers to the cloud holding the surfaces
		extract.setInputCloud(cloud_filtered);
		extract.setIndices(current_plane_indices);
		extract.setNegative(false);
		extract.filter(*cloud_planar_surface);

		// ... and remove those inliers from the input cloud
		extract.setNegative(true);
		extract.filter(*cloud_filtered);


/*
		if(first)
		{
			first=false;
			for (int i = 0; i < cloud_planar_surface->size(); i++)
			{
				cout << cloud_planar_surface->at(i).z << endl;
			}
			continue;
		}
*/
		
        *cloud_surfaces_ += *cloud_planar_surface;

		//vec_segments.push_back(cloud_planar_surface);
		//Classify the Cloud
		classify(cloud_planar_surface, coefficients);
        //vec_cloud_surfaces_.push_back(cloud_planar_surface);
	}

	//std::cout << "The number of surface groups: " << vec_segments.size()
	//		<< std::endl;
	//std::cout << "The number of coeffs: " << m_coefficients.size() << std::endl;
}

template<class PointT>
std::vector<pcl::PointIndices> SurfaceSegmenter<PointT>::getSurfaceClusters(
		PointCloudPtr const& cloud) {
	// Extract the clusters from such a filtered cloud.
	int max_size=cloud->points.size();
	clusterizer_.setClusterTolerance(0.03);
	clusterizer_.setMinClusterSize(1800+500);
	clusterizer_.setMaxClusterSize(max_size);
	pcl::search::KdTree<pcl::PointXYZ>::Ptr kd_tree_(
			new pcl::search::KdTree<pcl::PointXYZ>);
	kd_tree_->setInputCloud(cloud);
	clusterizer_.setSearchMethod(kd_tree_);
	clusterizer_.setInputCloud(cloud);
	std::vector<pcl::PointIndices> cluster_indices;
	clusterizer_.extract(cluster_indices);

	return cluster_indices;
}

template<class PointT> double SurfaceSegmenter<PointT>::getAngle(
		const pcl::ModelCoefficients &coeffs, const int &index) {

	//Scalar Product
	float scalar_product = (m_coefficients.at(index).values[0]
			* coeffs.values[0])
			+ (m_coefficients.at(index).values[1] * coeffs.values[1])
			+ (m_coefficients.at(index).values[2] * coeffs.values[2]);
	double angle = acos(scalar_product) * 180.0 / M_PI;
	//cout << "The angle from scalar product: " << angle << std::endl;

	return angle;
}


template<class PointT>
void SurfaceSegmenter<PointT>::classify(PointCloudPtr const& cloud_planar_surface,
		const pcl::ModelCoefficients & coeffs) {

	int size = m_coefficients.size();
	for (int i = 0; i < size; i++) {
		double angle = getAngle(coeffs, i);

		if (angle < 3 || angle > 177) {
			*vec_surface.at(i) += *cloud_planar_surface;
			return;
		}
	}
	vec_surface.push_back(cloud_planar_surface);
	m_coefficients.push_back(coeffs);
}



template<class PointT>
void SurfaceSegmenter<PointT>::cluster() {
	for (int i = 0; i < vec_surface.size(); i++) {
        std::vector<pcl::PointIndices> cluster_indices = getSurfaceClusters(vec_surface.at(i));
		clustersToPointClouds(i, cluster_indices);
	}
}

template<class PointT>
void SurfaceSegmenter<PointT>::clustersToPointClouds(int cloudIndex,
		std::vector<pcl::PointIndices> const& cluster_indices) {
	// Now copy the points belonging to each cluster to a separate PointCloud
	// and finally return a vector of these point clouds.
	//std::vector<PointCloudConstPtr> ret;
	size_t const cluster_count = cluster_indices.size();
	for (size_t i = 0; i < cluster_count; ++i) {

		PointCloudPtr current(new PointCloudT());
		std::vector<int> const& curr_indices = cluster_indices[i].indices;
		size_t const curr_indices_sz = curr_indices.size();
		for (size_t j = 0; j < curr_indices_sz; j++) {
			// add the point to the corresponding point cloud
			current->push_back(vec_surface.at(cloudIndex)->at(curr_indices[j]));
		}
        vec_cloud_surfaces_.push_back(current);
        //surfaceCoefficients.push_back(m_coefficients[cloudIndex]);
	}
    //std::cout << "surfaces number:" << vec_cloud_surfaces_.size() << std::endl;
}


template<class PointT>
void SurfaceSegmenter<PointT>::segment(
		const PointCloudConstPtr& cloud,
		std::vector<PointCloudConstPtr> &surfaces,
		PointCloudPtr &cloudMinusSurfaces,
		std::vector<pcl::ModelCoefficients> *&surfCoeff) {
	cloudMinusSurfaces = preprocessCloud(cloud);
    // extract those planes that are considered as surfaces and put them in cloud_surfaces_
    findSurfaces(cloudMinusSurfaces);
    cluster();
    surfaces = vec_cloud_surfaces_;
    surfCoeff = &m_coefficients;
}

} // namespace lepp

#endif
