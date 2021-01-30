#include <ros/ros.h>
#include <tf/tf.h>
#include <actionlib/server/simple_action_server.h>
#include <actionlib/client/simple_action_client.h>
#include <hector_moveit_actions/ExecuteDroneTrajectoryAction.h>

#include <geometry_msgs/Twist.h>
#include <airsim_ros_pkgs/VelCmd.h>
#include <geometry_msgs/Pose.h>
#include <nav_msgs/Odometry.h>

#include <hector_uav_msgs/PoseAction.h>
#include <hector_uav_msgs/EnableMotors.h>

#include <cmath>
#define _USE_MATH_DEFINES

#define MAX_SPEED 1.5
#define EPSILON 1e-4
class TrajectoryActionController{
    private:

        typedef actionlib::SimpleActionServer<hector_moveit_actions::ExecuteDroneTrajectoryAction> TrajectoryActionServer;
        typedef actionlib::SimpleActionClient<hector_uav_msgs::PoseAction> OrientationClient;
        typedef hector_moveit_actions::ExecuteDroneTrajectoryResult Result;
        typedef hector_moveit_actions::ExecuteDroneTrajectoryFeedback Feedback;
        ros::NodeHandle nh_;
        ros::Publisher vel_pub;
        ros::Subscriber pose_sub;
        TrajectoryActionServer server_;
        OrientationClient orientation_client_;
        std::string action_name;

        geometry_msgs::Twist empty,cmd;
        std::vector<geometry_msgs::Pose> trajectory;
        geometry_msgs::Pose last_pose;
       
        Feedback feedback_;
        Result result_;
        bool success, executing;
    public:
        TrajectoryActionController(std::string name) : action_name(name), 
            server_(nh_,name,boost::bind(&TrajectoryActionController::executeCB,this,_1),false),
            orientation_client_("/action/pose",true){
		    std::cout<<"waiting for orientation action server\n";
                //orientation_client_.waitForServer();
                empty.linear.x = 0;empty.linear.y = 0; empty.linear.z = 0;
                empty.angular.x = 0;empty.angular.y = 0;empty.angular.z = 0;
                vel_pub = nh_.advertise<airsim_ros_pkgs::VelCmd>("/airsim_node/SimpleFlight/vel_cmd_body_frame",10);
                //vel_pub = nh_.advertise<geometry_msgs::Twist>("/cmd_vel",10);
		std::cout<<"subscribe to odom_local_ned\n";
                pose_sub = nh_.subscribe<nav_msgs::Odometry>("/airsim_node/SimpleFlight/odom_local_ned",10,&TrajectoryActionController::poseCallback,this);
                //pose_sub = nh_.subscribe<nav_msgs::Odometry>("/ground_truth/state",10,&TrajectoryActionController::poseCallback,this);
                ros::ServiceClient enable_motors = nh_.serviceClient<hector_uav_msgs::EnableMotors>("/enable_motors");
                hector_uav_msgs::EnableMotors srv;
                srv.request.enable = true;
                if(enable_motors.call(srv)){
                    if(srv.response.success)
                        ROS_INFO("Motors are enabled");
                }
                success = true;
                executing = false;
                server_.start();
            
            }
	/* rostopic pub -r 10 airsim_node/SimpleFlight/vel_cmd_body_frame airsim_ros_pkgs/VelCmd "twist:   
	  linear:
    x: 4.0
    y: 0.0
    z: -1.0
  angular:
    x: 0.0
    y: 0.0
    z: 0.0" */
// to control airsim, we directly send to above topic. the command is w.r.t drone local frame. this topic is listened by airsim node
// goal is in world frame.
        void executeCB(const hector_moveit_actions::ExecuteDroneTrajectoryGoalConstPtr &goal){
            executing = true;
            trajectory = goal->trajectory;
            ROS_INFO_STREAM("Executing trajectory!");
	    std::cout<< "traj size " << trajectory.size() << std::endl;
            for(int i=0; i<trajectory.size()-1; i++){
                if(server_.isPreemptRequested() || !ros::ok()){
                    ROS_INFO("Preempt requested");
                    this->success = false;
                    executing = false;
                    break;
                }
		airsim_ros_pkgs::VelCmd vel_msg_airsim;
                geometry_msgs::Twist vel_msg;
                last_pose.position=trajectory[i+1].position;
                last_pose.orientation=trajectory[i+1].orientation;
//std::cout<<"traj i+1 " <<i+1<<std::endl;
//std::cout<<last_pose;
                feedback_.current_pose = last_pose;
                ros::spinOnce();
                ros::Duration(0.05).sleep();
std::cout<<"traj i+1 " <<i+1<<std::endl;
std::cout<<"curr pose " <<last_pose;
std::cout<<"traj i+1 :" <<trajectory[i+1];
                double goalx = trajectory[i+1].position.x;
                double goaly = trajectory[i+1].position.y;
                double goalz = trajectory[i+1].position.z;
                double diffx = goalx - last_pose.position.x;
                double diffy = goaly - last_pose.position.y;

                double diffz = goalz - last_pose.position.z;
                

                double step_angle = atan2(diffy,diffx);
                tf::Quaternion q;
                quaternionMsgToTF(last_pose.orientation,q);
                tf::Matrix3x3 m(q);
                double tmp,heading;
                m.getRPY(tmp,tmp,heading);

                ROS_INFO("Diffz: %lf",diffz);
                ROS_INFO("Step angle: %lf, heading: %lf",step_angle,heading);
                ros::Rate r(10);//airsim need 10hz for smooth control
                if(((fabs(diffx)>0.01 || fabs(diffy)>0.01) && fabs(step_angle-heading) > 0.3)){
                    hector_uav_msgs::PoseGoal goal;
                    ROS_INFO("Adjust orientation");
		    std::cout<<"Adjust orientation\n";
                    geometry_msgs::Pose p;
                    p.position.x = goalx;
                    p.position.y = goaly;
                    p.position.z = goalz;
                    tf::Quaternion q;
                    if(fabs(diffx)>0.01 || fabs(diffy)>0.01)
                        q = tf::createQuaternionFromYaw(step_angle);
                    else
                        q = tf::createQuaternionFromYaw(heading);    
                    p.orientation.x = q.x();
                    p.orientation.y = q.y();
                    p.orientation.z = q.z();
                    p.orientation.w = q.w();
                    server_.publishFeedback(feedback_);
                    goal.target_pose.pose = p;
                    goal.target_pose.header.frame_id="world";
                }
		//clime to the same z level
                while(fabs(diffz)>0.08){
                    vel_msg.linear.x = 0;
                    vel_msg.linear.z = diffz * 3;
		    std::cout<<"publish cmd_vel z axis\n";
		    vel_msg_airsim.twist=vel_msg;
                    vel_pub.publish(vel_msg_airsim);
                    //vel_pub.publish(vel_msg);
                    ros::spinOnce();
                    r.sleep();
                    diffz = goalz - last_pose.position.z;
                    server_.publishFeedback(feedback_);
                }
                
                
                double latched_distance = sqrt(pow(diffx,2) + pow(diffy,2));
                double distance = latched_distance;
                if(i>trajectory.size()-3) // Slow down at final waypoints.
                        vel_msg.linear.x /= 3;
                ROS_INFO("Computed distance: %lf",distance);
		double diffx_norm, diffy_norm;
                while(distance > 0.4*MAX_SPEED){
		    diffx_norm = diffx/(fabs(diffx)+fabs(diffy));
		    diffy_norm = diffy/(fabs(diffx)+fabs(diffy));
                    vel_msg.linear.y = diffy_norm * MAX_SPEED; vel_msg.linear.x= diffx_norm*MAX_SPEED,vel_msg.linear.z = 0;
                    
		    std::cout<<"publish cmd_vel "<<vel_msg <<" \n";
		    vel_msg_airsim.twist=vel_msg;
                    vel_pub.publish(vel_msg_airsim);
                    //vel_pub.publish(vel_msg);
                    ros::spinOnce();
                    r.sleep();
		    diffx=goalx- last_pose.position.x;
		    diffy=goaly- last_pose.position.y;
		    std::cout<<"diffx, diffy " << diffx<<diffy<<std::endl;
                    distance = sqrt(pow(goalx - last_pose.position.x,2) + pow(goaly - last_pose.position.y,2));
                    ROS_INFO("Distance to goal: %lf",distance);
                    server_.publishFeedback(feedback_);
                    if(distance < latched_distance) latched_distance = distance;
		    if (distance < 0.5) {
			    std::cout<<" close enough \n";
			    break;
		    }
                }
                
            }
            executing = false;
            if(!this->success){
                result_.result_code = Result::COLLISION_IN_FRONT;
                server_.setPreempted(result_);
                return ;
            }
            ROS_INFO_STREAM("Executed trajectory!");
            result_.result_code = Result::SUCCESSFUL;
            server_.setSucceeded(result_);
           
        }
        void idle(){
	    airsim_ros_pkgs::VelCmd vel_msg_airsim;
            while(ros::ok()){
                if(!executing){
		    vel_msg_airsim.twist=empty;
		    // airsim do not constant 0 velocity
                    //vel_pub.publish(vel_msg_airsim);
                    //vel_pub.publish(empty);
		}
                ros::spinOnce();
                ros::Duration(0.25).sleep();
            }
        }
        void actionCallback(const hector_uav_msgs::PoseFeedbackConstPtr& feedback,geometry_msgs::Pose& p){
            double euler_distance = pow(p.position.x - feedback->current_pose.pose.position.x,2) + pow(p.position.y - feedback->current_pose.pose.position.y,2)
                                    + pow(p.position.z - feedback->current_pose.pose.position.z,2);
            euler_distance = sqrt(euler_distance);
            if(euler_distance < 0.15)
                orientation_client_.cancelGoal();
        }
        void poseCallback(const nav_msgs::Odometry::ConstPtr & msg)
        {
//		std::cout<<"last_pose update\n";
            last_pose = msg->pose.pose;
        }

};



int main(int argc, char** argv)
{
  ros::init(argc, argv, "trajectory_executor");
  TrajectoryActionController controller("/action/trajectory");
  controller.idle();
  //ros::spin();
  return 0;
}
