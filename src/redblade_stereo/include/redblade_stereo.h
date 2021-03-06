#include <image_transport/image_transport.h>

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <fstream>
#include <stdint.h>
#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <geometry_msgs/Point.h>
#include <geometry_msgs/PointStamped.h>
#include <geometry_msgs/Pose2D.h>
#include <pcl_ros/point_cloud.h>
#include <pcl_conversions/pcl_conversions.h>
// PCL specific includes
//#include <pcl/ros/conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/passthrough.h>
#include <pcl/ModelCoefficients.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/conditional_removal.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/octree/octree.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>

#include <pcl/sample_consensus/ransac.h>
#include <pcl/sample_consensus/sac_model_line.h>

#define singleIZoneWidth  4     //Width of single snow field
#define tripleIZoneWidth  7     //Width of triple snow field
#define zoneLength    15   //Length of plowing zone
#define fieldLength   10   //Length of snow field


class redblade_stereo{
 public:
  double groundHeight;       //maximum height of ground
  double poleWidth;          //Width of the pole
  double towerWidth;         //Half of width of the tower
  double viewingRadius;      //Appropriate viewing radius of stereo camera.  Everything outside of this radius is filtered out
  double viewingWidth;       //Appropriate viewing width of stereo camera.  All periperals are filtered out
  double cameraHeight;       //Height of camera on robot
  double cameraLengthOffset; //Vertical offset of camera away from center of robot
  std::string surveyFile;
  std::vector<double> x;
  std::vector<double> y;

  double fieldAngle;
  bool tripleI;
  bool searchSnowField; //Indicates whether to search inside of the snow field or not
  

  redblade_stereo(std::string surveyFile,
		  bool tripleI,
		  bool searchSnowField,
		  double groundHeight, 
		  double poleWidth,
		  double cameraHeight,
		  double cameraLengthOffset);
  redblade_stereo(std::string surveyFile,
		  double groundHeight, 
		  double poleWidth,
		  double cameraHeight,
		  double cameraLengthOffset);
  redblade_stereo(double viewingRadius, 
		  double viewingWidth,
		  double groundHeight, 
		  double poleWidth,
		  double cameraHeight,
		  double cameraLengthOffset);
  ~redblade_stereo();

  void rotate(double& x, double& y);
  /*Check if points are within the survey field of interest*/
  bool inBounds(double x, double y);
  
  /*Check if transformed points are inside of the snow field*/
  bool inSnowField(double transformedX, double transformedY);

  //Transform point cloud from stereo camera coordinates to robot coordinates
  void transformStereo2Robot(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud);
  //Transform Point from robot coordinates to local ENU coordinates
  void transformRobot2ENU(geometry_msgs::Pose2D& currentPose,
			  geometry_msgs::Point& localPolePoint,
			  geometry_msgs::Point& enuPolePoint);
  void transformStereo2ENU(geometry_msgs::Pose2D& currentPose,
			   pcl::PointCloud<pcl::PointXYZ>::Ptr cloud);

  //Filters out ground using a passthrough filter
  void filterGround(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud,
		    pcl::PointCloud<pcl::PointXYZ>::Ptr filtered);
  //Filters out everything outside of 2 m
  void filterBackground(geometry_msgs::Pose2D pose,
			pcl::PointCloud<pcl::PointXYZ>::Ptr cloud,
			pcl::PointCloud<pcl::PointXYZ>::Ptr filtered);
  void filterRadius(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud,
		    pcl::PointCloud<pcl::PointXYZ>::Ptr filtered);
  //Finds the pole using the RANSAC algorithm
  void ransac(pcl::PointCloud<pcl::PointXYZ>::Ptr in,
	      pcl::PointCloud<pcl::PointXYZ>::Ptr pole,
	      Eigen::VectorXf& coeff);

  /*
    Performs Euclidean clustering
    Returns the total number of clusters

    Note: tolerance is in meters
    See pointclouds.org/documentation/tutorials/cluster_extraction.php
   */
  int cluster(pcl::PointCloud<pcl::PointXYZ>::Ptr in,double tolerance);

  //Returns a 2D point representation of the pole
  void cloud2point(pcl::PointCloud<pcl::PointXYZ>::Ptr pole,
		   geometry_msgs::Point& point);
  bool findPole(pcl::PointCloud<pcl::PointXYZ>::Ptr in,
		pcl::PointCloud<pcl::PointXYZ>::Ptr pole);
};
