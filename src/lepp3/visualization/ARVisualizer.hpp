#ifndef LEPP3_ARVISUALIZER_H__
#define LEPP3_ARVISUALIZER_H__

#include "lepp3/FrameData.hpp"
#include "lepp3/Typedefs.hpp"
#include "lepp3/models/ObjectModel.h"
#include "lepp3/models/SurfaceModel.h"

#include <am2b-arvis/ARVisualizer.hpp>
#include <vector>
#include <algorithm>
#include <iostream> 

namespace lepp 
{
/**
* Visitor class for obstacles.
*/
class ModelDrawer : public ModelVisitor 
{
public:
  ModelDrawer(ar::ARVisualizer *arvis, std::vector<mesh_handle_t> &visHandles) : arvis(arvis), visHandles(visHandles) {}
  virtual ~ModelDrawer() {}

  /**
  * Draw visited sphere.
  */
  virtual void visitSphere(lepp::SphereModel& sphere)
  {
    Coordinate const& center = sphere.center();
    double centerPoint[3] = {center.x, center.y, center.z};
    double radius = sphere.radius();
    ar::Sphere obstacle(centerPoint, radius, ar::Color(0,127,127,0.3));

    // sphere was not drawn before
    if (sphere.get_meshHandle() == -1)
    {
      mesh_handle_t mh = arvis->Add(obstacle);
      sphere.set_meshHandle(mh);
    }
    // update sphere
    else
      arvis->Update(sphere.get_meshHandle(), obstacle);

    // add handle to visHandles
    visHandles.push_back(sphere.get_meshHandle());
  }

  /**
  * Draw visited capsule.
  */
  virtual void visitCapsule(lepp::CapsuleModel& capsule)
  {
    // add capsule
    double center1[3] = {capsule.first().x, capsule.first().y, capsule.first().z};
    double center2[3] = {capsule.second().x, capsule.second().y, capsule.second().z};
    double radius = capsule.radius();
    ar::Capsule obstacle(center1, center2, radius, ar::Color(127,0,127,0.3));
    // capsule was not drawn before
    if (capsule.get_meshHandle() == -1)
    {
      mesh_handle_t mh = arvis->Add(obstacle);
      capsule.set_meshHandle(mh);
    }
    // update capsule
    else
      arvis->Update(capsule.get_meshHandle(), obstacle);

    // add handle to visHandles
    visHandles.push_back(capsule.get_meshHandle());
  }

private:
  /**
  * The instance to which the drawer will draw all models.
  */
  ar::ARVisualizer *arvis;

  /**
  * Vector of all handles that are visualized in this frame.
  */
  std::vector<mesh_handle_t> &visHandles;
};



/**
* Visitor class for surfaces.
*/
class SurfaceDrawer : public SurfaceVisitor
{
public:
  SurfaceDrawer(ar::ARVisualizer *arvis, std::vector<mesh_handle_t> &visHandles,
    std::vector<int> &usedColors) : 
    arvis(arvis), visHandles(visHandles), usedColors(usedColors) 
  {
    computeUnusedColors();
  }
  virtual ~SurfaceDrawer() {}

  /**
  * Draw the convex hull of the visited Surface.
  */
  virtual void visitSurface(SurfaceModel &plane)
  {
    PointCloudConstPtr hull = plane.get_hull();
    int numPoints = static_cast<unsigned int>(hull->size());
    double points[numPoints * 3];
    for (int i = 0; i < numPoints; i++)
    {
      const PointT &p = hull->at(i);
      points[3*i] = p.x;
      points[3*i+1] = p.y;
      points[3*i+2] = p.z;
    }

    // plane was not drawn before
    if (plane.get_meshHandle() == -1)
    {
      int colorID = 0;
      // if there is still an unused color
      if (unusedColors.size() > 0)
      {
        colorID = unusedColors.back();
        unusedColors.pop_back();
      }
      ar::Polygon surfPoly(points, numPoints, ar::Color(r[colorID],g[colorID],b[colorID],1));
      mesh_handle_t mh = arvis->Add(surfPoly);
      plane.set_meshHandle(mh);
      plane.set_colorID(colorID);
      std::cout << "Draw new with color " << colorID << std::endl;
    }
    // update plane
    else
    {
      int colorID = plane.get_colorID();
      if (colorID == -1)
        std::cout << "ERROR" << std::endl;
      std::cout << "Redraw with color " << colorID << std::endl;
      ar::Polygon surfPoly(points, numPoints, ar::Color(r[colorID],g[colorID],b[colorID],1));
      arvis->Update(plane.get_meshHandle(), surfPoly);
    }

    // add handle to visHandles
    visHandles.push_back(plane.get_meshHandle());
  }
private:
  /**
  * The instance to which the drawer will draw all models.
  */
  ar::ARVisualizer *arvis;

  // predefine colors
  static const int numColors = 6;
  static const int r[numColors];
  static const int b[numColors];
  static const int g[numColors];

  /**
  * Vector of all handles that are visualized in this frame.
  */
  std::vector<mesh_handle_t> &visHandles;

  // vector of all color IDs of visualized surfaces
  std::vector<int> &usedColors;
  std::vector<int> unusedColors;


  void computeUnusedColors()
  {
    for (int i = 0; i < numColors; i++)
      if (std::find(usedColors.begin(), usedColors.end(), i) == usedColors.end())
        unusedColors.push_back(i);
  }
};

const int SurfaceDrawer::r[numColors] = {255,   0,   0, 255, 255,   0};
const int SurfaceDrawer::b[numColors] = {  0, 255,   0, 255,   0, 255};
const int SurfaceDrawer::g[numColors] = {  0,   0, 255,   0, 255, 255};





/**
* Wrapper class for ARVisualizer. Does communication with library visualizer.
*/
class ARVisualizer : public FrameDataObserver  
{
public:
	ARVisualizer(bool visualizeSurfaces, bool visualizeObstacles) : 
    arvis(new ar::ARVisualizer()),
    visualizeSurfaces(visualizeSurfaces),
    visualizeObstacles(visualizeObstacles)
	{  
    // Updates the camera parameters used for rendering.
    // @position Position of the camera in world-coordinates
    // @forward  Vector pointing in the direction the camera is facing
    // @up       Orthogonal to Forward, defines the vertical axis for the camera
    //double position[3] = {0,0,0};
    //double forward[3] = {1,0,0};
    //double up[3] = {0,0,1};
    
		arvis->Start();
    //arvis->SetCameraPose(position, forward, up);   
	}
	~ARVisualizer()
	{
		arvis->Stop();
		delete arvis;
	}

  /**
  * Visualize obstacles and surfaces of the given frame.
  */
  virtual void updateFrame(FrameDataPtr frameData);

private:
  // instance of ARVisualizer
	ar::ARVisualizer *arvis;

  /**
  * Visualize convex hulls of surfaces in given vector with ARVisualizer.
  */
  void drawSurfaces(std::vector<SurfaceModelPtr> surfaces, std::vector<mesh_handle_t> &visHandles);

  /**
  * Visualize obstacles in given vector with ARVisualizer.
  */
  void drawObstacles(std::vector<ObjectModelPtr> obstacles, std::vector<mesh_handle_t> &visHandles);

  /**
  * Output the number of the frame.
  */
  void outputFrameNum(FrameDataPtr frameData);

  /**
  * Remove old obstacles and surfaces that are no longer visualized.
  */
  void removeOldSurfObst(std::vector<mesh_handle_t> &visHandles);

  // visulize obstacles and surfaces only if options were chosen in config file
  bool visualizeSurfaces;
  bool visualizeObstacles;

  // vector that holds the handles to all obstacles and surfaces that were visualized in the previous frame
  std::vector<mesh_handle_t> oldHandles;
};


void ARVisualizer::drawSurfaces(std::vector<SurfaceModelPtr> surfaces, std::vector<mesh_handle_t> &visHandles)
{
  std::vector<int> usedColors;
  for (size_t i = 0; i < surfaces.size(); i++)
    if (surfaces[i]->get_colorID() != -1)
      usedColors.push_back(surfaces[i]->get_colorID());

  // create surface drawer object
  SurfaceDrawer sd(arvis, visHandles, usedColors);

  // draw surfaces
  for (size_t i = 0; i < surfaces.size(); i++)
    surfaces[i]->accept(sd);
}


void ARVisualizer::drawObstacles(std::vector<ObjectModelPtr> obstacles, std::vector<mesh_handle_t> &visHandles)
{
  // create model drawer object
  ModelDrawer md(arvis, visHandles);

  // draw obstacles
  for (size_t i = 0; i < obstacles.size(); i++)
    obstacles[i]->accept(md);
}


void ARVisualizer::outputFrameNum(FrameDataPtr frameData)
{
  std::cout << "Frame " << frameData->frameNum << "    " 
    << "Ransac " << frameData->planeCoeffsIteration << "    " 
    << "RansacRef " << frameData->planeCoeffsReferenceFrameNum;
  if (visualizeSurfaces)
    std::cout << "    ";
  else
    std::cout << std::endl;
  
  
  if (visualizeSurfaces)
  {
    std::cout << "Surfaces " << frameData->surfaceDetectionIteration << "    "
      << "SurfacesRef " << frameData->surfaceReferenceFrameNum << std::endl;
  }
}


void ARVisualizer::removeOldSurfObst(std::vector<mesh_handle_t> &visHandles)
{
  // compare the newly visualized handles with the old ones. Remove all handles that appear
  // in the old handle list but not in the new one.
  std::sort(visHandles.begin(), visHandles.end());
  for (mesh_handle_t &mh : oldHandles)
  {
    // if mesh handle is not contained in newly visualized handles, remove it from visualizer
    if (!std::binary_search(visHandles.begin(), visHandles.end(), mh))
      arvis->Remove(mh);
  }
  oldHandles = visHandles;
}


void ARVisualizer::updateFrame(FrameDataPtr frameData)
{
  // visualize all obstacles and surfaces and store their handles
  std::vector<mesh_handle_t> visHandles;
  if (visualizeObstacles)
    drawObstacles(frameData->obstacles, visHandles);

  if (visualizeSurfaces)
    drawSurfaces(frameData->surfaces, visHandles);

  // Remove old obstacles and surfaces that are no longer visualized
  removeOldSurfObst(visHandles);

  // output frame num and surface frame num
  outputFrameNum(frameData);
}


} // namespace lepp
#endif