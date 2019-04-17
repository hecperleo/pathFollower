
//----------------------------------------------------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 Hector Perez Leon
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
// OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//----------------------------------------------------------------------------------------------------------------------

#include <upat_follower/follower.h>

namespace upat_follower {

Follower::Follower() : nh_(), pnh_("~") {
    // Parameters
    pnh_.getParam("uav_id", uav_id_);
    pnh_.getParam("debug", debug_);
    // Subscriptions
    sub_pose_ = nh_.subscribe("/uav_" + std::to_string(uav_id_) + "/ual/pose", 0, &Follower::ualPoseCallback, this);
    // Publishers
    pub_output_velocity_ = nh_.advertise<geometry_msgs::TwistStamped>("/upat_follower/follower/uav_" + std::to_string(uav_id_) + "/output_vel", 1000);
    // Services
    server_prepare_path_ = nh_.advertiseService("/upat_follower/follower/uav_" + std::to_string(uav_id_) + "/prepare_path", &Follower::preparePathCb, this);
    server_prepare_trajectory_ = nh_.advertiseService("/upat_follower/follower/uav_" + std::to_string(uav_id_) + "/prepare_trajectory", &Follower::prepareTrajectoryCb, this);
    // Debug follower
    if (debug_) {
        pub_point_look_ahead_ = nh_.advertise<geometry_msgs::PointStamped>("/upat_follower/follower/uav_" + std::to_string(uav_id_) + "/debug_point_look_ahead", 1000);
        pub_point_normal_ = nh_.advertise<geometry_msgs::PointStamped>("/upat_follower/follower/uav_" + std::to_string(uav_id_) + "/debug_point_normal", 1000);
        pub_point_search_normal_begin_ = nh_.advertise<geometry_msgs::PointStamped>("/upat_follower/follower/uav_" + std::to_string(uav_id_) + "/debug_point_search_begin", 1000);
        pub_point_search_normal_end_ = nh_.advertise<geometry_msgs::PointStamped>("/upat_follower/follower/uav_" + std::to_string(uav_id_) + "/debug_point_search_end", 1000);
    }
    capMaxVelocities();
}

Follower::Follower(int _uav_id, bool _debug) {
    debug_ = _debug;
    uav_id_ = _uav_id;
    capMaxVelocities();
}

Follower::~Follower() {
}

void Follower::updatePath(nav_msgs::Path _new_target_path) {
    target_path_ = _new_target_path;
}

void Follower::updateTrajectory(nav_msgs::Path _new_target_path, nav_msgs::Path _new_target_vel_path) {
    target_path_ = _new_target_path;
    target_vel_path_ = _new_target_vel_path;
}

bool Follower::updatePathCb(upat_follower::UpdatePath::Request &_req_path, upat_follower::UpdatePath::Response &_res_path) {
    updatePath(_req_path.new_target_path);
    return true;
}

bool Follower::updateTrajectoryCb(upat_follower::UpdateTrajectory::Request &_req_trajectory, upat_follower::UpdateTrajectory::Response &_res_trajectory) {
    updateTrajectory(_req_trajectory.new_target_path, _req_trajectory.new_target_vel_path);
    return true;
}

nav_msgs::Path Follower::preparePath(nav_msgs::Path _init_path, int _generator_mode, double _look_ahead, double _cruising_speed) {
    follower_mode_ = 0;
    upat_follower::Generator generator(vxy_, vz_up_, vz_dn_, debug_);
    generator.generatePath(_init_path, _generator_mode);
    look_ahead_ = _look_ahead;
    if (_cruising_speed > smallest_max_velocity_) cruising_speed_ = smallest_max_velocity_;
    if (_cruising_speed <= 0) cruising_speed_ = 0.1;
    target_path_ = generator.out_path_;
    return generator.out_path_;
}

nav_msgs::Path Follower::prepareTrajectory(nav_msgs::Path _init_path, std::vector<double> _max_vel_percentage) {
    follower_mode_ = 1;
    upat_follower::Generator generator(vxy_, vz_up_, vz_dn_, debug_);
    generator.generateTrajectory(_init_path, _max_vel_percentage);
    target_vel_path_ = generator.generated_path_vel_percentage_;
    target_vel_path_.header.frame_id = generator.out_path_.header.frame_id;
    for (int i = 0; i < generator.generated_max_vel_percentage_.size(); i++) {
        generated_max_vel_percentage_.push_back(generator.generated_max_vel_percentage_.at(i));
    }
    max_vel_ = generator.max_velocity_;
    target_path_ = generator.out_path_;
    return generator.out_path_;
}

bool Follower::preparePathCb(upat_follower::PreparePath::Request &_req_path, upat_follower::PreparePath::Response &_res_path) {
    _res_path.generated_path = preparePath(_req_path.init_path, _req_path.generator_mode.data, _req_path.look_ahead.data, _req_path.cruising_speed.data);

    return true;
}

bool Follower::prepareTrajectoryCb(upat_follower::PrepareTrajectory::Request &_req_trajectory, upat_follower::PrepareTrajectory::Response &_res_trajectory) {
    std::vector<double> vec_max_vel_percentage;
    for (int i = 0; i < _req_trajectory.max_vel_percentage.size(); i++) {
        vec_max_vel_percentage.push_back(_req_trajectory.max_vel_percentage.at(i).data);
    }
    _res_trajectory.generated_path = prepareTrajectory(_req_trajectory.init_path, vec_max_vel_percentage);

    return true;
}

void Follower::ualPoseCallback(const geometry_msgs::PoseStamped::ConstPtr &_ual_pose) {
    ual_pose_ = *_ual_pose;
}

void Follower::updatePose(const geometry_msgs::PoseStamped &_ual_pose) {
    ual_pose_ = _ual_pose;
}

void Follower::capMaxVelocities() {
    // Cap input max velocities with default PX4 max velocities
    if (vxy_ < mpc_xy_vel_max_[0]) vxy_ = mpc_xy_vel_max_[0];
    if (vxy_ > mpc_xy_vel_max_[1]) vxy_ = mpc_xy_vel_max_[1];
    if (vz_up_ < mpc_z_vel_max_up_[0]) vz_up_ = mpc_z_vel_max_up_[0];
    if (vz_up_ > mpc_z_vel_max_up_[1]) vz_up_ = mpc_z_vel_max_up_[1];
    if (vz_dn_ < mpc_z_vel_max_dn_[0]) vz_dn_ = mpc_z_vel_max_dn_[0];
    if (vz_dn_ > mpc_z_vel_max_dn_[1]) vz_dn_ = mpc_z_vel_max_dn_[1];
    std::vector<double> velocities;
    velocities.push_back(vxy_);
    velocities.push_back(vz_up_);
    velocities.push_back(vz_dn_);
    smallest_max_velocity_ = *std::min_element(velocities.begin(), velocities.end());
}

int Follower::calculatePosOnPath(Eigen::Vector3f _current_point, double _search_range, int _prev_normal_pos_on_path, nav_msgs::Path _path_search) {
    std::vector<double> vec_distances;
    int start_search_pos_on_path = calculateDistanceOnPath(_prev_normal_pos_on_path, -_search_range);
    int end_search_pos_on_path = calculateDistanceOnPath(_prev_normal_pos_on_path, _search_range);
    for (int i = start_search_pos_on_path; i < end_search_pos_on_path; i++) {
        Eigen::Vector3f target_path_point;
        target_path_point = Eigen::Vector3f(target_path_.poses.at(i).pose.position.x, target_path_.poses.at(i).pose.position.y, target_path_.poses.at(i).pose.position.z);
        vec_distances.push_back((target_path_point - _current_point).norm());
    }
    auto smallest_distance = std::min_element(vec_distances.begin(), vec_distances.end());
    int pos_on_path = smallest_distance - vec_distances.begin();

    return pos_on_path + start_search_pos_on_path;
}

int Follower::calculatePosLookAhead(int _pos_on_path) {
    int pos_look_ahead;
    std::vector<double> vec_distances;
    double temp_dist = 0.0;
    for (_pos_on_path; _pos_on_path < target_path_.poses.size() - 1; _pos_on_path++) {
        Eigen::Vector3f p1 = Eigen::Vector3f(target_path_.poses.at(_pos_on_path).pose.position.x, target_path_.poses.at(_pos_on_path).pose.position.y, target_path_.poses.at(_pos_on_path).pose.position.z);
        Eigen::Vector3f p2 = Eigen::Vector3f(target_path_.poses.at(_pos_on_path + 1).pose.position.x, target_path_.poses.at(_pos_on_path + 1).pose.position.y, target_path_.poses.at(_pos_on_path + 1).pose.position.z);
        temp_dist = temp_dist + (p2 - p1).norm();
        if (temp_dist < look_ahead_) {
            pos_look_ahead = _pos_on_path;
        } else {
            _pos_on_path = target_path_.poses.size();
        }
    }

    return pos_look_ahead;
}

double Follower::changeLookAhead(int _pos_on_path) {
    // ROS_WARN("la: %f, max: %f, %: %f", max_vel_ * generated_max_vel_percentage_[_pos_on_path], max_vel_, generated_max_vel_percentage_[_pos_on_path]);
    return max_vel_ * generated_max_vel_percentage_[_pos_on_path];
}

geometry_msgs::TwistStamped Follower::calculateVelocity(Eigen::Vector3f _current_point, int _pos_look_ahead) {
    geometry_msgs::TwistStamped out_vel;
    Eigen::Vector3f target_p, unit_vec, hypo_vec;
    target_p = Eigen::Vector3f(target_path_.poses.at(_pos_look_ahead).pose.position.x, target_path_.poses.at(_pos_look_ahead).pose.position.y, target_path_.poses.at(_pos_look_ahead).pose.position.z);
    double distance = (target_p - _current_point).norm();
    switch (follower_mode_) {
        case 0:
            unit_vec = (target_p - _current_point) / distance;
            unit_vec = unit_vec / unit_vec.norm();
            out_vel.twist.linear.x = unit_vec(0) * cruising_speed_;
            out_vel.twist.linear.y = unit_vec(1) * cruising_speed_;
            out_vel.twist.linear.z = unit_vec(2) * cruising_speed_;
            break;
        case 1:
            hypo_vec = (target_p - _current_point);
            out_vel.twist.linear.x = hypo_vec(0);
            out_vel.twist.linear.y = hypo_vec(1);
            out_vel.twist.linear.z = hypo_vec(2);
            // unit_vec = (target_p - _current_point) / distance;
            // unit_vec = unit_vec / unit_vec.norm();
            // out_vel.twist.linear.x = unit_vec(0) * cruising_speed_;
            // out_vel.twist.linear.y = unit_vec(1) * cruising_speed_;
            // out_vel.twist.linear.z = unit_vec(2) * cruising_speed_;
            break;
    }
    out_vel.header.frame_id = target_path_.header.frame_id;

    return out_vel;
}

int Follower::calculateDistanceOnPath(int _prev_normal_pos_on_path, double _meters) {
    int pos_equals_dist;
    double dist_to_front, dist_to_back, temp_dist;
    std::vector<double> vec_distances;
    Eigen::Vector3f p_prev = Eigen::Vector3f(target_path_.poses.at(_prev_normal_pos_on_path).pose.position.x, target_path_.poses.at(_prev_normal_pos_on_path).pose.position.y, target_path_.poses.at(_prev_normal_pos_on_path).pose.position.z);
    Eigen::Vector3f p_front = Eigen::Vector3f(target_path_.poses.front().pose.position.x, target_path_.poses.front().pose.position.y, target_path_.poses.front().pose.position.z);
    Eigen::Vector3f p_back = Eigen::Vector3f(target_path_.poses.back().pose.position.x, target_path_.poses.back().pose.position.y, target_path_.poses.back().pose.position.z);
    dist_to_front = (p_prev - p_front).norm();
    dist_to_back = (p_prev - p_back).norm();
    temp_dist = 0.0;
    if (_meters > 0) {
        if (_meters < dist_to_back) {
            for (int i = _prev_normal_pos_on_path; i < target_path_.poses.size() - 1; i++) {
                Eigen::Vector3f p1 = Eigen::Vector3f(target_path_.poses.at(i).pose.position.x, target_path_.poses.at(i).pose.position.y, target_path_.poses.at(i).pose.position.z);
                Eigen::Vector3f p2 = Eigen::Vector3f(target_path_.poses.at(i + 1).pose.position.x, target_path_.poses.at(i + 1).pose.position.y, target_path_.poses.at(i + 1).pose.position.z);
                temp_dist = temp_dist + (p2 - p1).norm();
                if (temp_dist < _meters) {
                    pos_equals_dist = i;
                } else {
                    i = target_path_.poses.size();
                }
            }
        } else {
            pos_equals_dist = target_path_.poses.size() - 1;
        }
    } else {
        if (_meters < dist_to_front) {
            pos_equals_dist = 0;
            for (int i = _prev_normal_pos_on_path; i >= 1; i--) {
                Eigen::Vector3f p1 = Eigen::Vector3f(target_path_.poses.at(i).pose.position.x, target_path_.poses.at(i).pose.position.y, target_path_.poses.at(i).pose.position.z);
                Eigen::Vector3f p0 = Eigen::Vector3f(target_path_.poses.at(i - 1).pose.position.x, target_path_.poses.at(i - 1).pose.position.y, target_path_.poses.at(i - 1).pose.position.z);
                temp_dist = temp_dist + (p1 - p0).norm();
                if (temp_dist < fabs(_meters / 2)) {
                    pos_equals_dist = i;
                } else {
                    i = 0;
                }
            }
        } else {
            pos_equals_dist = 0;
        }
    }

    return pos_equals_dist;
}

void Follower::prepareDebug(double _search_range, int _normal_pos_on_path, int _pos_look_ahead) {
    point_normal_.header.frame_id = point_look_ahead_.header.frame_id =
        point_search_normal_begin_.header.frame_id = point_search_normal_end_.header.frame_id =
            target_path_.header.frame_id;
    point_normal_.point = target_path_.poses.at(_normal_pos_on_path).pose.position;
    point_look_ahead_.point = target_path_.poses.at(_pos_look_ahead).pose.position;
    int start_search_pos_on_path = calculateDistanceOnPath(prev_normal_pos_on_path_, -_search_range);
    int end_search_pos_on_path = calculateDistanceOnPath(prev_normal_pos_on_path_, _search_range);
    point_search_normal_begin_.point = target_path_.poses.at(start_search_pos_on_path).pose.position;
    point_search_normal_end_.point = target_path_.poses.at(end_search_pos_on_path).pose.position;
}

void Follower::pubMsgs() {
    pub_output_velocity_.publish(out_velocity_);
    if (debug_) {
        pub_point_look_ahead_.publish(point_look_ahead_);
        pub_point_normal_.publish(point_normal_);
        pub_point_search_normal_begin_.publish(point_search_normal_begin_);
        pub_point_search_normal_end_.publish(point_search_normal_end_);
    }
}

geometry_msgs::TwistStamped Follower::getVelocity() {
    if (target_path_.poses.size() > 1) {
        Eigen::Vector3f current_point, target_path0_point;
        current_point = Eigen::Vector3f(ual_pose_.pose.position.x, ual_pose_.pose.position.y, ual_pose_.pose.position.z);
        target_path0_point = Eigen::Vector3f(target_path_.poses.at(0).pose.position.x, target_path_.poses.at(0).pose.position.y, target_path_.poses.at(0).pose.position.z);
        if ((current_point - target_path0_point).norm() < 1) {
            flag_run_ = true;
        }
        if (flag_run_) {
            double search_range_normal_pos = look_ahead_ * 1.5;
            int normal_pos_on_path = calculatePosOnPath(current_point, search_range_normal_pos, prev_normal_pos_on_path_, target_path_);
            if (follower_mode_ == 1) {
                double search_range_vel = look_ahead_ * 1.5;
                int normal_vel_on_path = calculatePosOnPath(current_point, search_range_vel, prev_normal_vel_on_path_, target_vel_path_);
                prev_normal_vel_on_path_ = normal_vel_on_path;
                look_ahead_ = changeLookAhead(normal_vel_on_path);
            }
            int pos_look_ahead = calculatePosLookAhead(normal_pos_on_path);
            out_velocity_ = calculateVelocity(current_point, pos_look_ahead);
            if (debug_) {
                prepareDebug(search_range_normal_pos, normal_pos_on_path, pos_look_ahead);
            }
            prev_normal_pos_on_path_ = normal_pos_on_path;
        }
    }
    return out_velocity_;
}

}  // namespace upat_follower