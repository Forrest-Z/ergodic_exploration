/**
 * @file exploration_node.cpp
 * @author Boston Cleek
 * @date 23 Oct 2020
 * @brief Ergodic exploration
 */

#include <iostream>
#include <ctime>
#include <chrono>

#include <ros/ros.h>
#include <geometry_msgs/Twist.h>
#include <nav_msgs/OccupancyGrid.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/LaserScan.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2/utils.h>
#include <gazebo_msgs/ModelStates.h>

#include <ergodic_exploration/cart.hpp>
#include <ergodic_exploration/omni.hpp>
#include <ergodic_exploration/ergodic_control.hpp>
#include <ergodic_exploration/dynamic_window.hpp>
#include <ergodic_exploration/mapping.hpp>

constexpr char LOGNAME[] = "ergodic exploration";

using arma::eye;
using arma::mat;
using arma::span;
using arma::vec;

using namespace ergodic_exploration;

// static GridMap grid;
// static vec pose = { 0.0, 0.0, 0.0 };
// static vec vb = { 0.0, 0.0, 0.0 };
//
// static sensor_msgs::LaserScan::ConstPtr scan;
//
// static bool scan_update = false;
// static bool odom_update = false;
// static bool map_update = false;
//
// template <typename modelT>
// bool validateControl(const modelT& model, const Collision& collision, const GridMap& grid,
//                      const vec x0, const vec& u, double dt, double horizon)
// {
//   vec x = x0;
//   RungeKutta rk4(dt);
//   const auto steps = static_cast<unsigned int>(horizon / std::abs(dt));
//
//   for (unsigned int i = 0; i < steps; i++)
//   {
//     x = rk4.step(model, x, u);
//     if (collision.collisionCheck(grid, x))
//     {
//       return false;
//     }
//   }
//
//   return true;
// }
//
// void scanCallback(const sensor_msgs::LaserScan::ConstPtr& msg)
// {
//   scan = msg;
//   // ROS_INFO("Laser max: %f Laser min: %f", msg->range_max, msg->range_min);
//   scan_update = true;
// }
//
// void odomCallback(const nav_msgs::Odometry& msg)
// {
//   vb(0) = msg.twist.twist.linear.x;
//   vb(1) = msg.twist.twist.linear.y;
//   vb(2) = msg.twist.twist.angular.z;
//
//   odom_update = true;
// }
//
// void modelCallBack(const gazebo_msgs::ModelStates& msg)
// {
//   // store names of all items in gazebo
//   std::vector<std::string> names = msg.name;
//
//   // index of robot
//   int robot_index = 0;
//
//   // find diff_drive robot
//   int ctr = 0;
//   for (const auto& item : names)
//   {
//     // check for robot
//     if (item == "nuridgeback")
//     {
//       robot_index = ctr;
//     }
//
//     ctr++;
//   }  // end loop
//
//   // pose of robot
//   const tf2::Quaternion quat(msg.pose[robot_index].orientation.x,
//                              msg.pose[robot_index].orientation.y,
//                              msg.pose[robot_index].orientation.z,
//                              msg.pose[robot_index].orientation.w);
//
//   pose(0) = msg.pose[robot_index].position.x;
//   pose(1) = msg.pose[robot_index].position.y;
//   pose(2) = tf2::getYaw(msg.pose[robot_index].orientation);
// }
//
// void mapCallback(const nav_msgs::OccupancyGrid::ConstPtr& msg)
// {
//   grid.update(msg);
//   grid.print();
//   map_update = true;
// }

int main(int argc, char** argv)
{
  ROS_INFO_STREAM_NAMED("main", "Starting exploration");
  ros::init(argc, argv, "exploration");
  ros::NodeHandle nh;

  // ros::Subscriber map_sub = nh.subscribe("map", 1, mapCallback);
  // ros::Subscriber scan_sub = nh.subscribe("scan", 1, scanCallback);
  // ros::Subscriber odom_sub = nh.subscribe("odom", 1, odomCallback);
  // ros::Subscriber model_sub = nh.subscribe("/gazebo/model_states", 1, modelCallBack);
  //
  // ros::Publisher map_pub = nh.advertise<nav_msgs::OccupancyGrid>("map_update", 1, true);
  // ros::Publisher cmd_pub = nh.advertise<geometry_msgs::Twist>("cmd_vel", 1);
  // ros::Publisher path_pub = nh.advertise<nav_msgs::Path>("trajectory", 1, true);
  // ros::Publisher target_pub = nh.advertise<visualization_msgs::MarkerArray>("target", 1, true);
  // ros::Publisher sample_pub = nh.advertise<visualization_msgs::Marker>("samples", 1, true);
  //
  //
  // arma::arma_rng::set_seed_random();
  //
  // //////////////////////////////////////////////////////////////////////////////
  // // TODO: Add noise to motion model
  // // motion model
  // Omni omni;
  // //////////////////////////////////////////////////////////////////////////////
  // // publish on cmd_vel at a constant frequency
  // const auto frequency = 20.0;
  // // EC validation
  // // const auto dt = 0.1;
  // // const auto horizon = 0.5;
  // //////////////////////////////////////////////////////////////////////////////
  // // grid
  // // const auto xmin = 0.0;
  // // const auto xmax = 1.0;
  // // const auto ymin = 0.0;
  // // const auto ymax = 1.0;
  // // const auto resolution = 0.05;
  // // const auto xsize = ergodic_exploration::axis_length(xmin, xmax, resolution);
  // // const auto ysize = ergodic_exploration::axis_length(ymin, ymax, resolution);
  // // std::vector<int8_t> map_data(xsize * ysize, 50);
  // //
  // // GridMap grid(xmin, xmax, ymin, ymax, resolution, map_data);
  // //////////////////////////////////////////////////////////////////////////////
  // // ** Testing **
  // // nav_msgs::OccupancyGrid grid_msg;
  // // grid_msg.header.frame_id = "map";
  // // grid_msg.info.resolution = resolution;
  // // grid_msg.info.width = xsize;
  // // grid_msg.info.height = ysize;
  // // grid_msg.info.origin.position.x = xmin;
  // // grid_msg.info.origin.position.y = ymin;
  // //
  // // // TODO: use TF listener to get transform or param server
  // // const mat Tbs = ergodic_exploration::transform2d(0.393, 0.0);
  // // OccupancyMapper mapper(Tbs);
  // //////////////////////////////////////////////////////////////////////////////
  // // collision
  // const auto boundary_radius = 0.7;
  // const auto search_radius = 1.0;
  // const auto obstacle_threshold = 0.2;
  // const auto occupied_threshold = 0.8;
  //
  // Collision collision(boundary_radius, search_radius, obstacle_threshold,
  //                     occupied_threshold);
  // //////////////////////////////////////////////////////////////////////////////
  // // ergodic control
  // const auto ec_dt = 0.1;
  // const auto ec_horizon = 5.0;
  // const auto num_samples = 1e4;
  // const auto buffer_size = 1e6;
  // const auto batch_size = 100;
  //
  // mat R(3, 3, arma::fill::zeros);
  // R(0, 0) = 1.0;
  // R(1, 1) = 1.0;
  // R(2, 2) = 1.0;
  //
  // // TODO:: get covariance from slam
  // // const mat Sigma = eye<mat>(2, 2) * 0.0025;
  // const mat Sigma = eye<mat>(2, 2) * 0.001;
  // // const mat Sigma2 = eye<mat>(2, 2) * 0.01 * 0.01;
  // //
  // // inv(Sigma).print();
  // // inv(Sigma2).print();
  // // square(inv(Sigma)).print();
  //
  //
  //
  // ErgodicControl ergodic_control(omni, collision, ec_dt, ec_horizon, num_samples, buffer_size,
  //                                batch_size, R);
  // //////////////////////////////////////////////////////////////////////////////
  // // dwa
  // // const auto dwa_dt = 0.2;
  // // const auto dwa_horizon = 1.0;
  // // const auto dwa_frequency = frequency;
  // //
  // // const auto acc_lim_x = 2.5;
  // // const auto acc_lim_y = 2.5;
  // // const auto acc_lim_th = 1.0;
  // //
  // // const auto max_vel_x = 1.0;
  // // const auto min_vel_x = -1.0;
  // //
  // // const auto max_vel_y = 1.0;
  // // const auto min_vel_y = -1.0;
  // //
  // // const auto max_rot_vel = 2.0;
  // // const auto min_rot_vel = -2.0;
  // //
  // // const auto vx_samples = 3;
  // // const auto vy_samples = 5;
  // // const auto vth_samples = 5;
  // //
  // // DynamicWindow dwa(dwa_dt, dwa_horizon, dwa_frequency, acc_lim_x, acc_lim_y, acc_lim_th,
  // //                   max_vel_x, min_vel_x, max_vel_y, min_vel_y, max_rot_vel, min_rot_vel,
  // //                   vx_samples, vy_samples, vth_samples);
  // //////////////////////////////////////////////////////////////////////////////
  // // target
  // Gaussian g1({ 2.5, 2.5 }, { 1.5, 1.5 });
  // Gaussian g2({ 8.5, 2.5 }, { 1.5, 1.5 });
  // Gaussian g3({ -5.0, 8.0 }, { 2.0, 2.0 });
  // Target target({ g1, g2 });
  //
  // visualization_msgs::MarkerArray marker_array;
  // target.markers(marker_array, "map");
  //
  // target_pub.publish(marker_array);
  // //////////////////////////////////////////////////////////////////////////////
  //
  // // vec u = ergodic_control.control(collision, grid, target, pose);
  //
  //
  // // vec x = { 3.0, 3.0, 3.0/2.0 * PI};
  // // vec x = { 3.0, 1.5, 0.0};
  // // vec x = { 2.5, 1.5, PI };
  // // vec u = { 0.0, 0.0, 0.0 };
  // // vec uref = { 0.7, 0.0, 0.2 };
  //
  // ros::Rate rate(frequency);
  // while (nh.ok())
  // {
  //   ros::spinOnce();
  //
  //   if (map_update)
  //   {
  //     // auto t_start = std::chrono::high_resolution_clock::now();
  //
  //     vec u = ergodic_control.control(grid, target, pose);
  //     // if (!validateControl(omni, collision, grid, x, u, dt, horizon))
  //     // {
  //     //   ROS_INFO_STREAM_NAMED("Collision detected! Enabling DWA!");
  //     //   u = dwa.control(collision, grid, x, vb, u);
  //     // }
  //
  //     // ROS_INFO_STREAM_NAMED(LOGNAME, "DWA!");
  //
  //     // vec u = dwa.control(collision, grid, pose, vb, uref);
  //     // u.print("u_t:");
  //
  //     // auto t_end = std::chrono::high_resolution_clock::now();
  //     // std::cout
  //     //     << "Hz: "
  //     //     << 1.0 / (std::chrono::duration<double, std::milli>(t_end - t_start).count() /
  //     //               1000.0)
  //     //     << std::endl;
  //
  //     geometry_msgs::Twist twist_msg;
  //     twist_msg.linear.x = u(0);
  //     twist_msg.linear.y = u(1);
  //     twist_msg.angular.z = u(2);
  //
  //     cmd_pub.publish(twist_msg);
  //
  //     nav_msgs::Path trajectory;
  //     ergodic_control.path(trajectory, "map");
  //
  //     path_pub.publish(trajectory);
  //
  //     visualization_msgs::Marker marker;
  //     ergodic_control.samples(marker, "map");
  //
  //     sample_pub.publish(marker);
  //
  //     // break;
  //   }
  //
  //   // if (scan_update && odom_update)
  //   // {
  //   //
  //   //   if (mapper.updateMap(grid, scan, pose))
  //   //   {
  //   //     grid_msg.data = grid.gridData();
  //   //     map_pub.publish(grid_msg);
  //   //   }
  //   //   else
  //   //   {
  //   //     ROS_ERROR_NAMED(LOGNAME, "Failed to update map");
  //   //   }
  //   //
  //   //   scan_update = false;
  //   //   odom_update = false;
  //   // }
  //
  //   rate.sleep();
  // }

  ros::waitForShutdown();
  ROS_INFO_STREAM_NAMED("main", "Shutting down.");
  return 0;
}
