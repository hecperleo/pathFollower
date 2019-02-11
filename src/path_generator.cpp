#include <uav_path_manager/path_generator.h>

PathGenerator::PathGenerator() : nh_() {
    // Services
    server_generate_path_ = nh_.advertiseService("/uav_path_manager/generator/generate_path", &PathGenerator::pathCallback, this);

    // Client to get parameters from mavros and required default values
    get_param_client_ = nh_.serviceClient<mavros_msgs::ParamGet>("mavros/param/get");
    mavros_params_["MPC_XY_VEL_MAX"] = 2.0;    // [m/s]   Default value
    mavros_params_["MPC_Z_VEL_MAX_UP"] = 3.0;  // [m/s]   Default value
    mavros_params_["MPC_Z_VEL_MAX_DN"] = 1.0;  // [m/s]   Default value
    mavros_params_["MC_YAWRATE_MAX"] = 200.0;  // [deg/s] Default value
    mavros_params_["MPC_TKO_SPEED"] = 1.5;     // [m/s]   Default value
    // Updating here is non-sense as service seems to be slow in waking up
}

PathGenerator::~PathGenerator() {
}

int PathGenerator::nearestNeighbourIndex(std::vector<double> &_x, double &_value) {
    double dist = std::numeric_limits<double>::max();
    double newDist = dist;
    size_t idx = 0;

    for (size_t i = 0; i < _x.size(); ++i) {
        newDist = std::abs(_value - _x[i]);
        if (newDist <= dist) {
            dist = newDist;
            idx = i;
        }
    }

    return idx;
}

std::vector<double> PathGenerator::linealInterp1(std::vector<double> &_x, std::vector<double> &_y, std::vector<double> &_x_new) {
    std::vector<double> y_new;
    double dx, dy, m, b;
    size_t x_max_idx = _x.size() - 1;
    size_t x_new_size = _x_new.size();

    y_new.reserve(x_new_size);

    for (size_t i = 0; i < x_new_size; ++i) {
        size_t idx = nearestNeighbourIndex(_x, _x_new[i]);

        if (_x[idx] > _x_new[i]) {
            dx = idx > 0 ? (_x[idx] - _x[idx - 1]) : (_x[idx + 1] - _x[idx]);
            dy = idx > 0 ? (_y[idx] - _y[idx - 1]) : (_y[idx + 1] - _y[idx]);
        } else {
            dx = idx < x_max_idx ? (_x[idx + 1] - _x[idx]) : (_x[idx] - _x[idx - 1]);
            dy = idx < x_max_idx ? (_y[idx + 1] - _y[idx]) : (_y[idx] - _y[idx - 1]);
        }

        m = dy / dx;
        b = _y[idx] - _x[idx] * m;

        y_new.push_back(_x_new[i] * m + b);
    }

    return y_new;
}

bool PathGenerator::pathCallback(uav_path_manager::GeneratePath::Request &_req_path,
                                 uav_path_manager::GeneratePath::Response &_res_path) {
    std::vector<double> list_pose_x, list_pose_y, list_pose_z;
    for (int i = 0; i < _req_path.init_path.poses.size(); i++) {
        list_pose_x.push_back(_req_path.init_path.poses.at(i).pose.position.x);
        list_pose_y.push_back(_req_path.init_path.poses.at(i).pose.position.y);
        list_pose_z.push_back(_req_path.init_path.poses.at(i).pose.position.z);
    }
    list_pose_x.push_back(list_pose_x.back());
    list_pose_y.push_back(list_pose_y.back());
    list_pose_z.push_back(list_pose_z.back());
    switch (_req_path.generator_mode.data) {
        case 1:
            mode_ = mode_interp1_;
            _res_path.generated_path = pathManagement(list_pose_x, list_pose_y, list_pose_z);
            break;
        case 2:
            mode_ = mode_cubic_spline_loyal_;
            _res_path.generated_path = pathManagement(list_pose_x, list_pose_y, list_pose_z);
            break;
        case 3:
            mode_ = mode_cubic_spline_;
            _res_path.generated_path = pathManagement(list_pose_x, list_pose_y, list_pose_z);
            break;
        case 4:
            mode_ = mode_trajectory_;
            std::vector<double> time_intervals;
            for (int i = 0; i < _req_path.time_intervals.size(); i++) {
                time_intervals.push_back(_req_path.time_intervals.at(i).data);
            }
            _res_path.generated_path = createTrajectory(list_pose_x, list_pose_y, list_pose_z, list_pose_x.size(), time_intervals);
            for (int i = 0; i < time_intervals.size(); i++) {
                for (int j = 0; j < (_res_path.generated_path.poses.size() / time_intervals.size()); j++) {
                    std_msgs::Float32 time_interval;
                    time_interval.data = time_intervals[i];
                    _res_path.generated_time_intervals.push_back(time_interval);
                }
            }
            break;
    }
    _res_path.generated_path.header.frame_id = _req_path.init_path.header.frame_id;

    return true;
}

std::vector<double> PathGenerator::interpWaypointList(std::vector<double> _list_pose_axis, int _amount_of_points) {
    std::vector<double> aux_axis;
    std::vector<double> new_aux_axis;
    for (int i = 0; i < _list_pose_axis.size(); i++) {
        aux_axis.push_back(i);
    }
    double portion = (aux_axis.back() - aux_axis.front()) / (_amount_of_points);
    double new_pose = aux_axis.front();
    new_aux_axis.push_back(new_pose);
    for (int i = 1; i < _amount_of_points; i++) {
        new_pose = new_pose + portion;
        new_aux_axis.push_back(new_pose);
    }
    auto interp1_path = linealInterp1(aux_axis, _list_pose_axis, new_aux_axis);
    return interp1_path;
}

nav_msgs::Path PathGenerator::constructPath(std::vector<double> _wps_x, std::vector<double> _wps_y, std::vector<double> _wps_z) {
    nav_msgs::Path path_msg;
    std::vector<geometry_msgs::PoseStamped> poses(_wps_x.size());
    for (int i = 0; i < _wps_x.size(); i++) {
        poses.at(i).pose.position.x = _wps_x[i];
        poses.at(i).pose.position.y = _wps_y[i];
        poses.at(i).pose.position.z = _wps_z[i];
        poses.at(i).pose.orientation.x = 0;
        poses.at(i).pose.orientation.y = 0;
        poses.at(i).pose.orientation.z = 0;
        poses.at(i).pose.orientation.w = 1;
    }
    path_msg.poses = poses;

    return path_msg;
}

nav_msgs::Path PathGenerator::createPathInterp1(std::vector<double> _list_x, std::vector<double> _list_y, std::vector<double> _list_z, int _path_size, int _new_path_size) {
    nav_msgs::Path interp1_path;
    std::vector<double> interp1_list_x, interp1_list_y, interp1_list_z;
    if (_path_size > 1) {
        // Lineal interpolation
        interp1_list_x = interpWaypointList(_list_x, _new_path_size);
        interp1_list_y = interpWaypointList(_list_y, _new_path_size);
        interp1_list_z = interpWaypointList(_list_z, _new_path_size);
        // Construct path
        interp1_path = constructPath(interp1_list_x, interp1_list_y, interp1_list_z);
    }

    return interp1_path;
}

nav_msgs::Path PathGenerator::createPathCubicSpline(std::vector<double> _list_x, std::vector<double> _list_y, std::vector<double> _list_z, int _path_size) {
    nav_msgs::Path cubic_spline_path;
    if (_path_size > 1) {
        // Calculate total distance
        int total_distance = 0;
        for (int i = 0; i < _path_size - 1; i++) {
            Eigen::Vector3f point_1, point_2;
            point_1 = Eigen::Vector3f(_list_x[i], _list_y[i], _list_z[i]);
            point_2 = Eigen::Vector3f(_list_x[i + 1], _list_y[i + 1], _list_z[i + 1]);
            total_distance = total_distance + (point_2 - point_1).norm();
        }
        // Calculate number of joints
        int num_joints = 0;
        switch (mode_) {
            case mode_cubic_spline_loyal_:
                num_joints = _path_size * 2;
                break;
            case mode_cubic_spline_:
                num_joints = _path_size;
                break;
            default:
                num_joints = total_distance;  // TODO: For trajectory generator
                break;
        }
        // Lineal interpolation
        std::vector<double> interp1_list_x, interp1_list_y, interp1_list_z;
        interp1_list_x = interpWaypointList(_list_x, num_joints);
        interp1_list_y = interpWaypointList(_list_y, num_joints);
        interp1_list_z = interpWaypointList(_list_z, num_joints);
        // Prepare sets for each cubic spline
        ecl::Array<double> t_set(interp1_list_x.size()), x_set(interp1_list_x.size()), y_set(interp1_list_x.size()), z_set(interp1_list_x.size());
        for (int i = 0; i < interp1_list_x.size(); i++) {
            x_set[i] = interp1_list_x[i];
            y_set[i] = interp1_list_y[i];
            z_set[i] = interp1_list_z[i];
            t_set[i] = (double)i;
        }
        // Create a cubic spline per axis
        ecl::CubicSpline spline_x = ecl::CubicSpline::Natural(t_set, x_set);
        ecl::CubicSpline spline_y = ecl::CubicSpline::Natural(t_set, y_set);
        ecl::CubicSpline spline_z = ecl::CubicSpline::Natural(t_set, z_set);
        // Change format: ecl::CubicSpline -> std::vector
        double sp_pts = total_distance;
        int _amount_of_points = (interp1_list_x.size() - 1) * sp_pts;
        std::vector<double> spline_list_x(_amount_of_points), spline_list_y(_amount_of_points), spline_list_z(_amount_of_points);
        for (int i = 0; i < _amount_of_points; i++) {
            spline_list_x[i] = spline_x(i / sp_pts);
            spline_list_y[i] = spline_y(i / sp_pts);
            spline_list_z[i] = spline_z(i / sp_pts);
        }
        // Construct path
        cubic_spline_path = constructPath(spline_list_x, spline_list_y, spline_list_z);
    }

    return cubic_spline_path;
}

nav_msgs::Path PathGenerator::createTrajectory(std::vector<double> _list_x, std::vector<double> _list_y, std::vector<double> _list_z, int _path_size, std::vector<double> _time_intervals) {
    nav_msgs::Path cubic_spline_path;
    if (_path_size > 1) {
        // Calculate total distance
        int total_distance = 0;
        for (int i = 0; i < _path_size - 1; i++) {
            Eigen::Vector3f point_1, point_2;
            point_1 = Eigen::Vector3f(_list_x[i], _list_y[i], _list_z[i]);
            point_2 = Eigen::Vector3f(_list_x[i + 1], _list_y[i + 1], _list_z[i + 1]);
            total_distance = total_distance + (point_2 - point_1).norm();
        }
        // Calculate number of joints
        int num_joints = total_distance;
        bool try_fit_spline = true;
        const double smallest_max_vel = checkSmallestMaxVel();
        while (try_fit_spline) {
            // Lineal interpolation
            std::vector<double> interp1_list_x, interp1_list_y, interp1_list_z;
            interp1_list_x = interpWaypointList(_list_x, num_joints);
            interp1_list_y = interpWaypointList(_list_y, num_joints);
            interp1_list_z = interpWaypointList(_list_z, num_joints);
            // Prepare sets for each cubic spline
            ecl::Array<double> t_set(interp1_list_x.size()), x_set(interp1_list_x.size()), y_set(interp1_list_x.size()), z_set(interp1_list_x.size());
            for (int i = 0; i < interp1_list_x.size(); i++) {
                x_set[i] = interp1_list_x[i];
                y_set[i] = interp1_list_y[i];
                z_set[i] = interp1_list_z[i];
                t_set[i] = (double)i;
            }
            // Create a cubic spline per axis
            ecl::CubicSpline spline_x = ecl::CubicSpline::Natural(t_set, x_set);
            ecl::CubicSpline spline_y = ecl::CubicSpline::Natural(t_set, y_set);
            ecl::CubicSpline spline_z = ecl::CubicSpline::Natural(t_set, z_set);
            // Change format: ecl::CubicSpline -> std::vector
            double sp_pts = total_distance;
            int _amount_of_points = (interp1_list_x.size() - 1) * sp_pts;
            std::vector<double> spline_list_x(_amount_of_points), spline_list_y(_amount_of_points), spline_list_z(_amount_of_points), vel_z_vec(_amount_of_points);
            for (int i = 0; i < _amount_of_points; i++) {
                spline_list_x[i] = spline_x(i / sp_pts);
                spline_list_y[i] = spline_y(i / sp_pts);
                spline_list_z[i] = spline_z(i / sp_pts);
                // TODO: Check in which axis is the smallest max velocity and use it.
                vel_z_vec[i] = spline_z.derivative(i / sp_pts);  // We use Z axis because we know that at this axis is the smallest max velocity.
            }
            double smallest_vel_z = *std::min_element(vel_z_vec.begin(), vel_z_vec.end());
            if (smallest_vel_z < smallest_max_vel) {
                num_joints++;
            } else {
                ROS_INFO("Generator -> smallest Z velocity: %f", smallest_vel_z);
                cubic_spline_path = constructPath(spline_list_x, spline_list_y, spline_list_z);
                try_fit_spline = false;
            }
        }
    }

    return cubic_spline_path;
}

nav_msgs::Path PathGenerator::pathManagement(std::vector<double> _list_pose_x, std::vector<double> _list_pose_y, std::vector<double> _list_pose_z) {
    const int interp1_final_size = 10000;
    switch (mode_) {
        case mode_interp1_:
            return createPathInterp1(_list_pose_x, _list_pose_y, _list_pose_z, _list_pose_x.size(), interp1_final_size);
        case mode_cubic_spline_loyal_:
            return createPathCubicSpline(_list_pose_x, _list_pose_y, _list_pose_z, _list_pose_x.size());
        case mode_cubic_spline_:
            return createPathCubicSpline(_list_pose_x, _list_pose_y, _list_pose_z, _list_pose_x.size());
            // case mode_trajectory_:
            //     return createTrajectory(_list_pose_x, _list_pose_y, _list_pose_z, _list_pose_x.size());
    }
}

double PathGenerator::checkSmallestMaxVel() {
    double mpc_xy_vel_max = updateParam("MPC_XY_VEL_MAX");
    double mpc_z_vel_max_up = updateParam("MPC_Z_VEL_MAX_UP");
    double mpc_z_vel_max_dn = updateParam("MPC_Z_VEL_MAX_DN");
    mpc_z_vel_max_dn = - mpc_z_vel_max_dn;
    std::vector<double> velocities;
    velocities.push_back(mpc_xy_vel_max);
    velocities.push_back(mpc_z_vel_max_up);
    velocities.push_back(mpc_z_vel_max_dn);

    return *std::min_element(velocities.begin(), velocities.end());;
}

double PathGenerator::updateParam(const std::string &_param_id) {
    mavros_msgs::ParamGet get_param_service;
    get_param_service.request.param_id = _param_id;
    if (get_param_client_.call(get_param_service) && get_param_service.response.success) {
        mavros_params_[_param_id] = get_param_service.response.value.integer ? get_param_service.response.value.integer : get_param_service.response.value.real;
        ROS_INFO("Parameter [%s] value is [%f]", get_param_service.request.param_id.c_str(), mavros_params_[_param_id]);
    } else if (mavros_params_.count(_param_id)) {
        ROS_ERROR("Error in get param [%s] service calling, leaving current value [%f]",
                  get_param_service.request.param_id.c_str(), mavros_params_[_param_id]);
    } else {
        mavros_params_[_param_id] = 0.0;
        ROS_ERROR("Error in get param [%s] service calling, initializing it to zero",
                  get_param_service.request.param_id.c_str());
    }
    return mavros_params_[_param_id];
}