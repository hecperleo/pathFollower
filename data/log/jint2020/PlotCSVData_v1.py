import pandas as pd
import numpy as np
import itertools
import os
from scipy import interpolate
from matplotlib import pyplot as plt
import matplotlib.ticker as ticker
from mpl_toolkits.mplot3d import Axes3D

''' Get directory '''
dir_config = '/home/hector/ros/ual_ws/src/upat_follower/config/'
dir_data = '/home/hector/ros/ual_ws/src/upat_follower/data/'
experiment_name = 'jint2020/2UAV-4D-1Conflict'
case_name = 'uav_1-2020-09-23_11-37-23'
dir_experiment = dir_data + 'log/' + experiment_name + '/' + case_name + '/'
dir_save_data = dir_data + 'img/' + experiment_name + '/' + case_name + '/'
''' Create folder to save data '''
if not os.path.exists(dir_save_data):
    os.makedirs(dir_save_data)
''' Get csv files '''
try:
    default_init_path = pd.read_csv(
        dir_experiment + 'init_waypoints.csv', names=['x', 'y', 'z', 'Times'])
except FileNotFoundError:
    print('init.csv not found!')
try:
    generated_trajectory_m0 = pd.read_csv(
        dir_experiment + 'generated_trajectory_m0.csv', names=['x', 'y', 'z'])
except FileNotFoundError:
    print('generated_trajectory_m0.csv not found!')
try:
    generated_trajectory_m1 = pd.read_csv(
        dir_experiment + 'generated_trajectory_m1.csv', names=['x', 'y', 'z'])
except FileNotFoundError:
    print('generated_trajectory_m1.csv not found!')
try:
    generated_trajectory_m2 = pd.read_csv(
        dir_experiment + 'generated_trajectory_m2.csv', names=['x', 'y', 'z'])
except FileNotFoundError:
    print('generated_trajectory_m2.csv not found!')
try:
    normal_dist_trajectory_m0 = pd.read_csv(
        dir_experiment + 'normal_dist_trajectory_m0.csv', names=['curTime', 'desTime', 'Spline', 'Linear', 'PosX', 'PosY', 'PosZ', 'curVelx', 'curVely', 'curVelz', 'desVelx', 'desVely', 'desVelz'])
except FileNotFoundError:
    print('normal_dist_trajectory_m0.csv not found!')
try:
    normal_dist_trajectory_m1 = pd.read_csv(
        dir_experiment + 'normal_dist_trajectory_m1.csv', names=['curTime', 'desTime', 'Spline', 'Linear', 'PosX', 'PosY', 'PosZ', 'curVelx', 'curVely', 'curVelz', 'desVelx', 'desVely', 'desVelz'])
except FileNotFoundError:
    print('normal_dist_trajectory_m1.csv not found!')
try:
    normal_dist_trajectory_m2 = pd.read_csv(
        dir_experiment + 'normal_dist_trajectory_m2.csv', names=['curTime', 'desTime', 'Spline', 'Linear', 'PosX', 'PosY', 'PosZ', 'curVelx', 'curVely', 'curVelz', 'desVelx', 'desVely', 'desVelz'])
except FileNotFoundError:
    print('normal_dist_trajectory_m2.csv not found!')
try:
    current_trajectory_m0 = pd.read_csv(
        dir_experiment + 'current_trajectory_m0.csv', names=['x', 'y', 'z'])
except FileNotFoundError:
    print('current_trajectory_m0.csv not found!')
try:
    current_trajectory_m1 = pd.read_csv(
        dir_experiment + 'current_trajectory_m1.csv', names=['x', 'y', 'z'])
except FileNotFoundError:
    print('current_trajectory_m1.csv not found!')
try:
    current_trajectory_m2 = pd.read_csv(
        dir_experiment + 'current_trajectory_m2.csv', names=['x', 'y', 'z'])
except FileNotFoundError:
    print('current_trajectory_m2.csv not found!')
try:
    reach_times_trajectory_m0 = pd.read_csv(
        dir_experiment + 'reach_times_trajectory_m0.csv', names=['curTime'])
except FileNotFoundError:
    print('reach_times_trajectory_m0.csv not found!')
try:
    reach_times_trajectory_m1 = pd.read_csv(
        dir_experiment + 'reach_times_trajectory_m2.csv', names=['curTime'])
except FileNotFoundError:
    print('reach_times_trajectory_m2.csv not found!')
try:
    reach_times_trajectory_m2 = pd.read_csv(
        dir_experiment + 'reach_times_trajectory_m1.csv', names=['curTime'])
except FileNotFoundError:
    print('reach_times_trajectory_m1.csv not found!')


def getTimesWPsReached(_init_path, _normal_dist_trajectory):
    min_dist = 1000000
    times_wps_reached = []
    idx = jdx = 0
    for i in _init_path.values:
        p1 = np.asarray([_init_path.values[idx, 0],
                         _init_path.values[idx, 1], _init_path.values[idx, 2]])
        min_dist = 1000000
        jdx = 0
        for j in _normal_dist_trajectory.values:
            p2 = np.asarray([_normal_dist_trajectory.values[jdx, 4],
                             _normal_dist_trajectory.values[jdx, 5], _normal_dist_trajectory.values[jdx, 6]])
            temp_dist = np.linalg.norm(p2 - p1)
            if temp_dist < min_dist:
                min_dist = temp_dist
                t_wp_reached = _normal_dist_trajectory.values[jdx, 0]
            if jdx < _normal_dist_trajectory.shape[0]-1:
                jdx += 1
        times_wps_reached.append(t_wp_reached)
        idx += 1
    return times_wps_reached


def getModVelocity(_normal_dist_trajectory):
    mod_cur_vel = []
    mod_des_vel = []
    idx = 0
    for i in _normal_dist_trajectory.values:
        mod_cur_vel.append(np.sqrt(_normal_dist_trajectory.values[idx, 7] * _normal_dist_trajectory.values[idx, 7] +
                                   _normal_dist_trajectory.values[idx, 8] * _normal_dist_trajectory.values[idx, 8] +
                                   _normal_dist_trajectory.values[idx, 9] * _normal_dist_trajectory.values[idx, 9]))
        mod_des_vel.append(np.sqrt(_normal_dist_trajectory.values[idx, 10] * _normal_dist_trajectory.values[idx, 10] +
                                   _normal_dist_trajectory.values[idx, 11] * _normal_dist_trajectory.values[idx, 11] +
                                   _normal_dist_trajectory.values[idx, 12] * _normal_dist_trajectory.values[idx, 12]))
        idx += 1
    return mod_cur_vel, mod_des_vel


def getDesiredTimesForNonTrajectory(_default_times, _normal_dist_trajectory):
    generated_times = []
    array_len_ndist = []
    array_len_times = []
    def_times = []
    idx = 0
    for i in _normal_dist_trajectory.values:
        array_len_ndist.append(idx)
        idx += 1
    idx = 0
    for i in _default_times.values:
        array_len_times.append(idx)
        idx += 1
    idx = 0
    for i in _default_times.values:
        def_times.append(_default_times.values[idx, 0])
        idx += 1

    # generated_times = np.interp(array_len_ndist, array_len_times, def_times)
    def_times = [0.0, 30.0, 14.0, 42.0]
    test_array = np.linspace(0.0, 42.0, 1118)
    array_len_times = np.linspace(0.0, 42, 4)
    # test_array = np.arange(0.0, 42.0, 42.0/1118.0)
    print(len(test_array))
    # print (array_len_ndist, len(array_len_ndist))
    print(array_len_times, len(array_len_times))
    print(def_times, len(def_times))
    # x2 = def_times
    # y2 = array_len_times
    # xinterp = np.arange(len(_normal_dist_trajectory))
    # yinterp1 = np.interp(xinterp, x2, y2)
    generated_times = np.interp(test_array, array_len_times, def_times)
    plt.plot(generated_times)
    plt.show()
    print(generated_times)
    # generated_times = interpolate.interp1d(array_len_times, def_times, array_len_ndist)º
    # generated_times = yinterp1

    return generated_times


def plot3DFigure(_compare_path, _current_trajectory, _num):
    figN = plt.figure(num='Mode ' + str(_num) +
                      ' 3D behaviour', figsize=(6, 4))
    axN = Axes3D(figN)
    axN.plot(default_init_path.x, default_init_path.y, default_init_path.z, 'ko',
             #  color="0.5"
             )
    # ax1.plot(default_init_path.x, default_init_path.y, default_init_path.z, 'y')
    axN.plot(_compare_path.x, _compare_path.y, _compare_path.z, 'r--',
             #  color="0"
             )
    axN.plot(_current_trajectory.x, _current_trajectory.y,
             _current_trajectory.z, 'b', alpha=0.9
             #  color="0.4"
             )
    axN.legend(['Waypoints', 'Generated trajectory', 'Actual trajectory'])
    axN.set_xlim(35, 45)
    axN.set_ylim(-60, 40)
    axN.set_zlim(3, 7)
    axN.set_xlabel('X axis')
    axN.set_ylabel('Y axis')
    axN.set_zlabel('Z axis')
    figN.savefig(dir_save_data + '3D_traj_m' +
                 str(_num) + '.eps', format='eps', dpi=1200, bbox_inches="tight")
    return figN


def plot2DFigures(_normal_dist_trajectory, _times_wps_reached, _default_times, _num):
      
    plt.figure(num='Mode ' + str(_num) + ' normal distance', figsize=(6, 3))
    plt.plot(_normal_dist_trajectory.curTime,
             _normal_dist_trajectory.Spline, 'b', label="Normal distance to trajectory")
    plt.xlabel('Time (s)')
    plt.ylabel('Distance (m)')
    # plt.ylim(top=2)
    # plt.axes().xaxis.set_major_locator(ticker.MultipleLocator(5))
    # idx = 0
    # for xc in _default_times:
    #     plt.axvline(x=xc, color='r', linestyle='--', alpha=0.7,
    #                 label='WP ' + str(idx+1) + ' desired time ' +
    #                 str(_default_times[idx])
    #                 )
    #     idx += 1
    idx = 0
    for xc in _times_wps_reached:
        plt.axvline(x=xc, color='grey', linestyle='--', alpha=0.7,
                    label='WP ' + str(idx+1) + ' reached at ' +
                    str(_times_wps_reached[idx])
                    )
        idx += 1

    # str_times_wps_reached = "WPs reached at " + str(_times_wps_reached[0]) + "s, " + str(_times_wps_reached[1]) + "s, " + str(_times_wps_reached[2]) + "s, " + str(_times_wps_reached[3]) + "s"
    # str_default_times = "WPs desired time " + str(_default_times[0, 0]) + "s, " + str(_default_times[1, 0]) + "s, " + str(_default_times[2, 0]) + "s, " + str(_default_times[3, 0]) + "s"
    # plt.legend(['Normal distance to trajectory', str_default_times,
    #             str_times_wps_reached], fontsize='small')
    # plt.gca().get_legend().legendHandles[0].set_color('blue')
    # plt.gca().get_legend().legendHandles[1].set_color('green')
    # plt.gca().get_legend().legendHandles[2].set_color('red')
    plt.legend(['Normal distance', 'WP reached'])
    # plt.legend(fontsize='x-small')
    plt.savefig(dir_save_data + 'ndist_traj_m' +
                str(_num)+'.eps', format='eps', dpi=1200, bbox_inches='tight')
    plt.show(block=False)
    ''' Plot Velocities trajectory m0 '''
    mod_cur_vel = []
    mod_des_vel = []
    mod_cur_vel, mod_des_vel = getModVelocity(_normal_dist_trajectory)
    plt.figure(num='Velocities trajectory mode '+str(_num), figsize=(6, 3))
    plt.plot(_normal_dist_trajectory.curTime,
             mod_cur_vel, label="Current |v|", color='b')
    plt.plot(_normal_dist_trajectory.curTime, mod_des_vel,
             label="Desired |v|", color='r', alpha=0.7)
    idx = 0
    for xc in _times_wps_reached:
        plt.axvline(x=xc, color='grey', linestyle='--', alpha=0.7, label='WP ' + str(idx+1) + ' reached: ' +
                    str(_times_wps_reached[idx]))
        idx += 1
    plt.xlabel('Time (s)')
    plt.ylabel('Velocity (m/s)')
    plt.legend(['Current |v|', 'Desired |v|', 'WP reached'])
    plt.savefig(dir_save_data + 'vel_traj_m' +
                str(_num)+'.eps', format='eps', dpi=1200, bbox_inches='tight')
    plt.show(block=False)
    plt.figure(num='Delta of Times mode '+str(_num), figsize=(6, 3))
    plt.plot(_normal_dist_trajectory.curTime,
             _normal_dist_trajectory.desTime - _normal_dist_trajectory.curTime, color='b')
    idx = 0
    for xc in _times_wps_reached:
        plt.axvline(x=xc, color='grey', linestyle='--', alpha=0.7)
        idx += 1
    plt.xlabel('Current time (s)')
    plt.ylabel('Desired time - Current time (s)')
    # plt.ylim(bottom=-6)
    plt.legend(['Difference of times', 'WP reached'])
    plt.savefig(dir_save_data + 'deltaT_traj_m' +
                str(_num)+'.eps', format='eps', dpi=1200, bbox_inches='tight')
    plt.show(block=False)


if 'default_init_path' in globals():
    if 'normal_dist_trajectory_m0' in globals():
        times_wps_reached_m0 = getTimesWPsReached(
            default_init_path, normal_dist_trajectory_m0)
        plot3DFigure(generated_trajectory_m0, current_trajectory_m0, 0)
        plot2DFigures(normal_dist_trajectory_m0,
                      times_wps_reached_m0, default_init_path.Times, 0)
        plt.show(block=False)
        ''' Print results of the normal distance through path '''
        print(
            '-----------------------------------------------------------------------------')
        print('Trajectory m0 -> max: {:.3f}, min: {:.3f}, mean: {:.3f}, std: {:.3f}, var: {:.3f}'.format(np.max(normal_dist_trajectory_m0.Linear), np.min(
            normal_dist_trajectory_m0.Linear), np.mean(normal_dist_trajectory_m0.Linear), np.std(normal_dist_trajectory_m0.Linear), np.var(normal_dist_trajectory_m0.Linear)))
    if 'normal_dist_trajectory_m1' in globals():
        times_wps_reached_m1 = getTimesWPsReached(
            default_init_path, normal_dist_trajectory_m1)
        plot3DFigure(generated_trajectory_m1, current_trajectory_m1, 1)
        plot2DFigures(normal_dist_trajectory_m1,
                      times_wps_reached_m1, default_init_path.Times, 1)
        plt.show(block=True)
        print(
            '-----------------------------------------------------------------------------')
        print('Trajectory m1 -> max: {:.3f}, min: {:.3f}, mean: {:.3f}, std: {:.3f}, var: {:.3f}'.format(np.max(normal_dist_trajectory_m1.Spline), np.min(
            normal_dist_trajectory_m1.Spline), np.mean(normal_dist_trajectory_m1.Spline), np.std(normal_dist_trajectory_m1.Spline), np.var(normal_dist_trajectory_m1.Spline)))
    if 'normal_dist_trajectory_m2' in globals():
        times_wps_reached_m2 = getTimesWPsReached(
            default_init_path, normal_dist_trajectory_m2)
        plot3DFigure(generated_trajectory_m2, current_trajectory_m2, 2)
        plot2DFigures(normal_dist_trajectory_m2,
                      times_wps_reached_m2, default_init_path.Times, 2)
        plt.show(block=True)
        print(
            '-----------------------------------------------------------------------------')
        print('Trajectory m2 -> max: {:.3f}, min: {:.3f}, mean: {:.3f}, std: {:.3f}, var: {:.3f}'.format(np.max(normal_dist_trajectory_m2.Spline), np.min(
            normal_dist_trajectory_m2.Spline), np.mean(normal_dist_trajectory_m2.Spline), np.std(normal_dist_trajectory_m2.Spline), np.var(normal_dist_trajectory_m2.Spline)))
print('-----------------------------------------------------------------------------')
print('Times -> max: {:.3f}, min: {:.3f}, mean: {:.3f}, std: {:.3f}, var: {:.3f}'.format(np.max(np.abs(normal_dist_trajectory_m0.desTime - normal_dist_trajectory_m0.curTime)), 
                                                                                         np.min(np.abs(normal_dist_trajectory_m0.desTime - normal_dist_trajectory_m0.curTime)),
                                                                                         np.mean(np.abs(normal_dist_trajectory_m0.desTime - normal_dist_trajectory_m0.curTime)), 
                                                                                         np.std(np.abs(normal_dist_trajectory_m0.desTime - normal_dist_trajectory_m0.curTime)),
                                                                                         np.var(np.abs(normal_dist_trajectory_m0.desTime - normal_dist_trajectory_m0.curTime))))
print('-----------------------------------------------------------------------------')
plt.show(block=True)