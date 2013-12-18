/*
 * Translates sensor_msgs/NavSat{Fix,Status} into nav_msgs/Odometry using UTM
 */

#include <ros/ros.h>
#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <sensor_msgs/NavSatStatus.h>
#include <sensor_msgs/NavSatFix.h>
#include <gps_common/conversions.h>
#include <nav_msgs/Odometry.h>
#include <fstream>
#include <string>

using namespace gps_common;

static ros::Publisher odom_pub;
std::string frame_id, child_frame_id;
std::string filepath, filename1, filename2;
double rot_cov;
double initial_e, initial_n;
int numCorners = 2;

void initializeAndConvCorners(){
  double northing, easting, latitude, longitude;
  std::string zone;
  bool initialized = false;

  std::ifstream fs1(filename1.c_str(),std::ifstream::in);
  std::ofstream fs2(filename2.c_str(),std::ofstream::out);
  
  if(!fs1.good()){
    ROS_WARN("ERROR OPENING FILE 1 INPUT");
    return;
  }
  if(!fs2.good()){
    ROS_WARN("ERROR OPENING FILE 2 OUTPUT");
    return;
  }

  //set float precision to exactly 10
  fs1.unsetf(std::ofstream::floatfield);
  fs1.precision(10);
  fs1.setf(std::ofstream::fixed,std::ofstream::floatfield);
  fs2.unsetf(std::ofstream::floatfield);
  fs2.precision(10);
  fs2.setf(std::ofstream::fixed,std::ofstream::floatfield);

  std::string line, comma;
  while(std::getline(fs1,line)){
    std::istringstream ss(line);
    ss >> latitude >> comma >> longitude;

    LLtoUTM(latitude, longitude, northing, easting, zone);

    std::cout<< "LATLONG: " << latitude << "," << longitude << std::endl;
    
    if(!initialized){
      initialized = true;
      initial_e = easting;
      initial_n = northing;
    }

    easting = easting - initial_e;
    northing = northing - initial_n;
    fs2 << easting << ", " << northing << std::endl; 
    ROS_INFO("WROTE AN ENU POINT");
  }
  
  fs1.close();
  fs2.close();
}


void callback(const sensor_msgs::NavSatFixConstPtr& fix) {
  if (fix->status.status == sensor_msgs::NavSatStatus::STATUS_NO_FIX) {
    ROS_INFO("No fix.");
    return;
  }

  if (fix->header.stamp == ros::Time(0)) {
    return;
  }

  double northing, easting;
  std::string zone;

  LLtoUTM(fix->latitude, fix->longitude, northing, easting, zone);

  if (odom_pub) {
    nav_msgs::Odometry odom;
    odom.header.stamp = fix->header.stamp;

    if (frame_id.empty())
      odom.header.frame_id = fix->header.frame_id;
    else
      odom.header.frame_id = frame_id;

    odom.child_frame_id = child_frame_id;

    odom.pose.pose.position.x = easting - initial_e;
    odom.pose.pose.position.y = northing - initial_n;
    odom.pose.pose.position.z = fix->altitude;
    
    odom.pose.pose.orientation.x = 1;
    odom.pose.pose.orientation.y = 0;
    odom.pose.pose.orientation.z = 0;
    odom.pose.pose.orientation.w = 0;
    
    // Use ENU covariance to build XYZRPY covariance
    boost::array<double, 36> covariance = {{
      fix->position_covariance[0],
      fix->position_covariance[1],
      fix->position_covariance[2],
      0, 0, 0,
      fix->position_covariance[3],
      fix->position_covariance[4],
      fix->position_covariance[5],
      0, 0, 0,
      fix->position_covariance[6],
      fix->position_covariance[7],
      fix->position_covariance[8],
      0, 0, 0,
      0, 0, 0, rot_cov, 0, 0,
      0, 0, 0, 0, rot_cov, 0,
      0, 0, 0, 0, 0, rot_cov
    }};

    odom.pose.covariance = covariance;

    odom_pub.publish(odom);
  }
}

int main (int argc, char **argv) {  
  ros::init(argc, argv, "gps_conv_node");
  ros::NodeHandle node;
  ros::NodeHandle priv_node("~");

  priv_node.param<std::string>("frame_id", frame_id, "");
  priv_node.param<std::string>("child_frame_id", child_frame_id, "");
  priv_node.param<double>("rot_covariance", rot_cov, 99999.0);

  std::string skip;
  priv_node.param("filepath", filepath, skip);

  //create filename strings
  filename1 = filepath+"survey_geodetic.csv";
  filename2 = filepath+"survey_enu.csv";

  //delete files if the already exist
  std::remove(filename2.c_str());

  //initialize east/north and convert field corners to ENU
  initializeAndConvCorners();

  odom_pub = node.advertise<nav_msgs::Odometry>("odom", 10);

  ros::Subscriber fix_sub = node.subscribe("fix", 10, callback);
  
  ros::spin();
}
