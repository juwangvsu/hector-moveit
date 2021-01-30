#! /usr/bin/env python

import rospy
#from __future__ import print_function

# Brings in the SimpleActionClient
import actionlib

# Brings in the messages used by the fibonacci action, including the
# goal message and the result message.
import hector_moveit_actions.msg
from geometry_msgs.msg import Pose
def fibonacci_client():
    # Creates the SimpleActionClient, passing the type of the action
    # (FibonacciAction) to the constructor.
    client = actionlib.SimpleActionClient('/action/trajectory', hector_moveit_actions.msg.ExecuteDroneTrajectoryAction)

    # Waits until the action server has started up and started
    # listening for goals.
    client.wait_for_server()

    # Creates a goal to send to the action server.
    goal = hector_moveit_actions.msg.ExecuteDroneTrajectoryGoal()
    pose1=Pose()
    pose1.position.x=1
    pose1.position.y=1
    pose1.position.z=-27
    pose1.orientation.x=0
    pose1.orientation.y=0
    pose1.orientation.z=0
    pose1.orientation.w=1
    goal.trajectory.append(pose1)
    pose1.position.x=2
    goal.trajectory.append(pose1)
    pose1.position.x=13
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)
    goal.trajectory.append(pose1)

    # Sends the goal to the action server.
    client.send_goal(goal)

    # Waits for the server to finish performing the action.
    client.wait_for_result()

    # Prints out the result of executing the action
    return client.get_result()  # A FibonacciResult

if __name__ == '__main__':
    try:
        # Initializes a rospy node so that the SimpleActionClient can
        # publish and subscribe over ROS.
        rospy.init_node('fibonacci_client_py')
        result = fibonacci_client()
        print("Result:", result)
    except rospy.ROSInterruptException:
        print("program interrupted before completion")
