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
#include <geometry_msgs/TransformStamped.h>
#include <nav_msgs/OccupancyGrid.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/LaserScan.h>
#include <tf2_ros/transform_listener.h>
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

static GridMap grid;
static vec pose = { 0.0, 0.0, 0.0 };
static vec vb = { 0.0, 0.0, 0.0 };

static sensor_msgs::LaserScan::ConstPtr scan;

static bool scan_update = false;
static bool odom_update = false;
static bool map_received = false;

bool validateControl(const Collision& collision, const GridMap& grid,
                     const vec& u, double dt, double horizon)
{
  vec x = pose;
  const vec delta = integrate_twist(x, u, dt);
  const auto steps = static_cast<unsigned int>(std::abs(horizon / dt));

  for (unsigned int i = 0; i < steps; i++)
  {
    x += delta;
    if (collision.collisionCheck(grid, x))
    {
      return false;
    }
  }

  return true;
}

void scanCallback(const sensor_msgs::LaserScan::ConstPtr& msg)
{
  scan = msg;
  // ROS_INFO("Laser max: %f Laser min: %f", msg->range_max, msg->range_min);
  scan_update = true;
}

void odomCallback(const nav_msgs::Odometry& msg)
{
  vb(0) = msg.twist.twist.linear.x;
  vb(1) = msg.twist.twist.linear.y;
  vb(2) = msg.twist.twist.angular.z;

  odom_update = true;
}

void modelCallBack(const gazebo_msgs::ModelStates& msg)
{
  // store names of all items in gazebo
  std::vector<std::string> names = msg.name;

  // index of robot
  int robot_index = 0;

  // find diff_drive robot
  int ctr = 0;
  for (const auto& item : names)
  {
    // check for robot
    if (item == "nuridgeback")
    {
      robot_index = ctr;
    }

    ctr++;
  }

  // pose(0) = msg.pose[robot_index].position.x;
  // pose(1) = msg.pose[robot_index].position.y;
  // pose(2) = tf2::getYaw(msg.pose[robot_index].orientation);

  // std::cout << "Pose gazebo: " << msg.pose[robot_index].position.x <<
  // " " << msg.pose[robot_index].position.y << " " << tf2::getYaw(msg.pose[robot_index].orientation) << std::endl;
}

void mapCallback(const nav_msgs::OccupancyGrid::ConstPtr& msg)
{
  grid.update(msg);
  grid.print();
  map_received = true;
}

int main(int argc, char** argv)
{
  ROS_INFO_STREAM_NAMED("main", "Starting exploration");
  ros::init(argc, argv, "exploration");
  ros::NodeHandle nh;

  ros::Subscriber map_sub = nh.subscribe("map", 1, mapCallback);
  ros::Subscriber scan_sub = nh.subscribe("scan", 1, scanCallback);
  ros::Subscriber odom_sub = nh.subscribe("odom", 1, odomCallback);
  ros::Subscriber model_sub = nh.subscribe("/gazebo/model_states", 1, modelCallBack);

  // ros::Publisher map_pub = nh.advertise<nav_msgs::OccupancyGrid>("map_update", 1, true);
  ros::Publisher cmd_pub = nh.advertise<geometry_msgs::Twist>("cmd_vel", 1);
  ros::Publisher path_pub = nh.advertise<nav_msgs::Path>("trajectory", 1, true);
  ros::Publisher target_pub =
      nh.advertise<visualization_msgs::MarkerArray>("target", 1, true);

  tf2_ros::Buffer tfBuffer;
  tf2_ros::TransformListener tfListener(tfBuffer);
  geometry_msgs::TransformStamped t_map_base;

  arma::arma_rng::set_seed_random();

  //////////////////////////////////////////////////////////////////////////////
  // TODO: Add noise to motion model
  // motion model
  Omni omni;
  //////////////////////////////////////////////////////////////////////////////
  // publish on cmd_vel at a constant frequency
  const auto frequency = 10.0;
  // EC validation
  const auto dt = 0.1;
  const auto horizon = 0.5;

  const auto max_vel_x = 1.0;
  const auto min_vel_x = -1.0;

  const auto max_vel_y = 1.0;
  const auto min_vel_y = -1.0;

  const auto max_rot_vel = 2.0;
  const auto min_rot_vel = -2.0;

  const vec umin = { min_vel_x, min_vel_y, min_rot_vel };
  const vec umax = { max_vel_x, max_vel_y, max_rot_vel };
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
  // collision
  const auto boundary_radius = 0.7;
  const auto search_radius = 1.0;
  const auto obstacle_threshold = 0.2;
  const auto occupied_threshold = 0.8;

  Collision collision(boundary_radius, search_radius, obstacle_threshold,
                      occupied_threshold);
  // //////////////////////////////////////////////////////////////////////////////
  // ergodic control
  const auto ec_dt = 0.1;
  const auto ec_horizon = 2.0;
  const auto target_resolution = 0.1;
  const auto expl_weight = 1.0;
  const auto num_basis = 10;
  const auto buffer_size = 1e6;
  const auto batch_size = 100;

  mat R(3, 3, arma::fill::zeros);
  R(0, 0) = 1.0;
  R(1, 1) = 1.0;
  R(2, 2) = .5;

  ErgodicControl ergodic_control(omni, collision, ec_dt, ec_horizon, target_resolution,
                                 expl_weight, num_basis, buffer_size, batch_size, R, umin, umax);
  // //////////////////////////////////////////////////////////////////////////////
  // // dwa
  const auto dwa_dt = 0.1;
  const auto dwa_horizon = 1.5;
  const auto dwa_frequency = frequency;

  const auto acc_lim_x = 2.5;
  const auto acc_lim_y = 2.5;
  const auto acc_lim_th = 1.0;

  const auto vx_samples = 3;
  const auto vy_samples = 8;
  const auto vth_samples = 5;

  DynamicWindow dwa(collision, dwa_dt, dwa_horizon, dwa_frequency, acc_lim_x, acc_lim_y,
                    acc_lim_th, max_vel_x, min_vel_x, max_vel_y, min_vel_y, max_rot_vel,
                    min_rot_vel, vx_samples, vy_samples, vth_samples);
  // //////////////////////////////////////////////////////////////////////////////
  // target
  Gaussian g1({ 2.5, 2.5 }, { 1.5, 1.5 });
  Gaussian g2({ 8.5, 2.5 }, { 1.5, 1.5 });
  // Gaussian g3({ -5.0, 8.0 }, { 2.0, 2.0 });
  Target target({ g2 });

  ergodic_control.setTarget(target);

  visualization_msgs::MarkerArray marker_array;
  target.markers(marker_array, "map");

  target_pub.publish(marker_array);
  // //////////////////////////////////////////////////////////////////////////////

  // // vec u = ergodic_control.control(collision, grid, target, pose);
  //
  //
  // // vec x = { 3.0, 3.0, 3.0/2.0 * PI};
  // // vec x = { 3.0, 1.5, 0.0};
  // // vec x = { 2.5, 1.5, PI };
  vec u = { 0.0, 0.0, 0.0 };
  // vec uref = { 0.7, 0.0, 0.0 };

  bool pose_known = false;
  ros::Rate rate(frequency);
  while (nh.ok())
  {
    ros::spinOnce();

    // Update pose
    // TODO: read frames from param server
    try
    {
      t_map_base = tfBuffer.lookupTransform("map", "base_link", ros::Time(0));
      pose(0) = t_map_base.transform.translation.x;
      pose(1) = t_map_base.transform.translation.y;
      pose(2) = tf2::getYaw(t_map_base.transform.rotation); // wrapped -PI to PI ?
      pose_known = true;
    }

    catch (tf2::TransformException &ex)
    {
      ROS_WARN_NAMED(LOGNAME, "%s", ex.what());
      continue;
    }

    // pose_known = true;


    // Contol loop
    if (map_received && pose_known)
    {
      // auto t_start = std::chrono::high_resolution_clock::now();

      u = ergodic_control.control(grid, pose);
      if (!validateControl(collision, grid, u, dt, horizon))
      {
        ROS_INFO_STREAM_NAMED(LOGNAME, "Collision detected! Enabling DWA!");
        u = dwa.control(grid, pose, vb, u);
      }

      // ROS_INFO_STREAM_NAMED(LOGNAME, "DWA!");

      // vec u = dwa.control(grid, pose, vb, uref);
      // u.print("u_t:");

      // auto t_end = std::chrono::high_resolution_clock::now();
      // std::cout
      //     << "Hz: "
      //     << 1.0 / (std::chrono::duration<double, std::milli>(t_end - t_start).count() /
      //               1000.0)
      //     << std::endl;

      geometry_msgs::Twist twist_msg;
      twist_msg.linear.x = u(0);
      twist_msg.linear.y = u(1);
      twist_msg.angular.z = u(2);

      cmd_pub.publish(twist_msg);

      nav_msgs::Path trajectory;
      ergodic_control.path(trajectory, "map");

      path_pub.publish(trajectory);

      // break;
    }

    // if (scan_update && odom_update)
    // {
    //
    //   if (mapper.updateMap(grid, scan, pose))
    //   {
    //     grid_msg.data = grid.gridData();
    //     map_pub.publish(grid_msg);
    //   }
    //   else
    //   {
    //     ROS_ERROR_NAMED(LOGNAME, "Failed to update map");
    //   }
    //
    //   scan_update = false;
    //   odom_update = false;
    // }

    rate.sleep();
  }

  ros::waitForShutdown();
  ROS_INFO_STREAM_NAMED("main", "Shutting down.");
  return 0;
}
