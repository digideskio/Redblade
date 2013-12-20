//#include <ros/ros.h>
#include <geometry_msgs/Twist.h>
#include <string>
#include <cmath>
//#include "ax2550/StampedEncoders.h"
#include <tf/tf.h>
#include "odometry_skid_steer.h"

ros::Time prev_time;
nav_msgs::Odometry odom;
ros::Publisher odom_pub;
std::string odom_frame_id;
bool imu_init = false;
geometry_msgs::Vector3 orientation;
//geometry_msgs::Vector3 prev_orientation;

ax2550::StampedEncoders front_encoders,back_encoders;

bool front_recv = false;
bool back_recv = false;

void frontEncoderCallback(const ax2550::StampedEncoders& msg){
  front_encoders = msg;
  front_recv = true;
}

void backEncoderCallback(const ax2550::StampedEncoders& msg){
  back_encoders = msg;
  back_recv = true;
}

void imuCallback(const geometry_msgs::Vector3::ConstPtr& msg){
  if(!imu_init){
    imu_init = true;    
  }
  orientation.x = msg->x;
  orientation.y = msg->y;
  orientation.z = msg->z;
}

odometry_skid_steer::odometry_skid_steer(std::string odom_frame_id, double rot_cov_, double pos_cov_,double wheel_base_width){
  odom_frame_id= odom_frame_id;
  x_pos = 0, y_pos = 0, theta = 0;
  x_vel = 0, y_vel = 0, theta_vel = 0;
  rot_cov = rot_cov_;
  pos_cov = pos_cov_;
  prev_fr_encoder = 0, prev_fl_encoder= 0, prev_br_encoder= 0, prev_bl_encoder= 0;
  prev_orientation.x = 0;
  prev_orientation.y = 0;
  prev_orientation.z = 0;
  wheel_base_width = wheel_base_width;
}

odometry_skid_steer::~odometry_skid_steer(){

}


void odometry_skid_steer::getVelocities(const ax2550::StampedEncoders& front_msg,
					const ax2550::StampedEncoders& back_msg,
					geometry_msgs::Twist& twist){
  double delta_time,left_encoders,right_encoders;
  getEncoders(front_msg,back_msg,left_encoders,right_encoders,delta_time);
  double Vr = right_encoders/delta_time;
  double Vl = left_encoders/delta_time;
  double Vx = (Vr+Vl)/2;
  double w  = (Vr-Vl)/wheel_base_width;
  twist.linear.x = Vx;
  twist.linear.y = 0;
  twist.linear.z = 0;
  twist.angular.x = 0;
  twist.angular.y = 0;
  twist.angular.z = w;
}

void publish_loop(odometry_skid_steer odomSS){
  //publish odom messages
  nav_msgs::Odometry odom = odomSS.getOdometry(front_encoders,back_encoders,orientation);
  odom_pub.publish(odom);  
}  


int main(int argc, char** argv){
  ros::init(argc, argv, "odometry_skid_steer");
  ros::NodeHandle n; //in the global namespace
  ros::NodeHandle nh("~");//local namespace, used for params
  std::string front_encoder_namespace,back_encoder_namespace;
  double rot_cov_,pos_cov_;
  double wheel_base_width;
  //See odometry_skid_steer.h for all constants
  n.param("front_encoders", front_encoder_namespace, std::string("/front_encoders"));
  n.param("back_encoders", back_encoder_namespace, std::string("/back_encoders"));
  n.param("rotation_covariance",rot_cov_, 1.0);
  n.param("position_covariance",pos_cov_, 1.0);
  n.param("odom_frame_id", odom_frame_id, std::string("odom"));
  n.param("wheel_base_width", wheel_base_width, 0.473);
  odometry_skid_steer odomSS(odom_frame_id,rot_cov_,pos_cov_,wheel_base_width);

  //Start Spinner so that encoder Callbacks happen in a seperate thread
  ros::AsyncSpinner spinner(2);
  spinner.start();

  //Subscribe to front/back encoder topics
  ros::Subscriber front_encoder_sub = n.subscribe(front_encoder_namespace, 1, 
  						  frontEncoderCallback);
  ros::Subscriber back_encoder_sub = n.subscribe(back_encoder_namespace, 1, 
  						 backEncoderCallback);
  odom_pub = n.advertise<geometry_msgs::Twist>("roboteq_front/cmd_vel", 10);
  
  //Set up rate for cmd_vel topic to be published at
  ros::Rate cmd_vel_rate(40);//Hz

  //publish cmd_vel topic every 25 ms (40 hz)
  while(ros::ok()){
      if(front_recv and back_recv){
  	publish_loop(odomSS);
      }
    //sleep for a bit to stay at 40 hz
    cmd_vel_rate.sleep();
  }

  spinner.stop();
  return(0);
}