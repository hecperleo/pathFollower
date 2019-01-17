#include <ros/ros.h>
#include <uav_abstraction_layer/ual.h>
#include <Eigen/Eigen>
#include "geometry_msgs/PoseStamped.h"
#include "geometry_msgs/TwistStamped.h"
#include "nav_msgs/Path.h"
#include <uav_path_manager/GetGeneratedPath.h>

class PathFollower {
   public:
    PathFollower();
    ~PathFollower();

    void pubMsgs();
    void followPath();

   private:
    // Callbacks
    void ualPoseCallback(const geometry_msgs::PoseStamped::ConstPtr &_ual_pose);
    // Methods
    int calculatePosOnPath(Eigen::Vector3f current_p);
    int calculatePosLookAhead(int pos_on_path);
    geometry_msgs::TwistStamped calculateVelocity(Eigen::Vector3f current_p, int pos_la);
    // Node handlers
    ros::NodeHandle nh;
    // Subscribers
    ros::Subscriber sub_pose;
    // Publishers
    ros::Publisher pub_output_vel;
    // Services
    ros::ServiceClient srv_get_generated_path;
    // Variables
    bool flag_run = false;
    double look_ahead = 1.0;
    nav_msgs::Path path;
    geometry_msgs::PoseStamped ual_pose;
    geometry_msgs::TwistStamped out_velocity;
};