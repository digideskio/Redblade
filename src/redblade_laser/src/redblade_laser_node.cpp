#include "redblade_laser.h"

ros::Publisher pub;
sensor_msgs::LaserScan currentScan;
geometry_msgs::Pose2D currentPose;
laser_geometry::LaserProjection projector_;
bool hasPoints;
bool hasPose;

redblade_laser* redLazer;

void pose_callback(const geometry_msgs::Pose2D::ConstPtr& pose_msg){
  currentPose = *pose_msg;
  hasPose = true;
  //ROS_INFO("Got pose");
}

void scanCallback (const sensor_msgs::LaserScan::ConstPtr& scan_in)
{
  currentScan = *scan_in;
  hasPoints = true;
}

void publish_loop(){
  sensor_msgs::PointCloud2 cloud;
  boost::shared_ptr<pcl::PointCloud<pcl::PointXYZ> > 
    pcl_cloud(new pcl::PointCloud<pcl::PointXYZ>());
  boost::shared_ptr<pcl::PointCloud<pcl::PointXYZ> > 
    filtered(new pcl::PointCloud<pcl::PointXYZ>());
  if(hasPoints and hasPose){
    projector_.projectLaser(currentScan, cloud); 
    pcl::fromROSMsg(cloud,*pcl_cloud);
    redLazer->transformLaser2ENU(currentPose,pcl_cloud);
    redLazer->filterBackground(pcl_cloud,filtered);
    redLazer->addScan(filtered);
    //ROS_INFO("Size of filtered cloud: %d",filtered->points.size());
    if(redLazer->saturated()){
      geometry_msgs::Point pt;
      geometry_msgs::PointStamped enuStamped;
      bool foundPole = redLazer->findPole(pt,0.1);//10 cm
      if(foundPole){
	enuStamped.header.stamp = ros::Time::now();
	enuStamped.header.frame_id = "laser";
	enuStamped.point = pt;
	pub.publish(enuStamped);
	hasPoints = false;
	hasPose = false;
      }
    }
  }
}

int main(int argc, char** argv){
  ros::init(argc, argv, "redblade_laser");
  ros::NodeHandle nh;
  ros::NodeHandle n("~"); 
  std::string laser_namespace,pole_namespace,ekf_namespace;
  hasPoints = false;
  hasPose = false;
  std::string surveyFile;
  int queue_size;
  double laser_length_offset;
  n.param("queue_size", queue_size, 5);
  n.param("laser_namespace", laser_namespace, std::string("/scan"));
  n.param("ekf_namespace", ekf_namespace, std::string("/redblade_ekf/2d_pose"));
  n.param("pole_namespace", pole_namespace, std::string("/lidar/pole"));
  n.param("survey_file", surveyFile, std::string("~"));
  n.param("laser_length_offset", laser_length_offset, 0.27);//TODO: Need more accurate measurement
  ROS_INFO("Laser namespace: %s",laser_namespace.c_str());
  redLazer = new redblade_laser(surveyFile,laser_length_offset,queue_size);
  
  ros::Subscriber scan_sub = nh.subscribe<sensor_msgs::LaserScan> (laser_namespace, queue_size, scanCallback);
  ros::Subscriber pose_sub  = nh.subscribe<geometry_msgs::Pose2D> (ekf_namespace, 1, pose_callback);
  pub = nh.advertise<geometry_msgs::PointStamped> (pole_namespace, 1);
  
  ros::AsyncSpinner spinner(2);
  spinner.start();    
  while(ros::ok()){ 
    publish_loop();
  }
  spinner.stop();  
}
