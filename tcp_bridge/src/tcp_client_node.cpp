#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/float64.hpp"
#include "std_msgs/msg/int32.hpp"
#include "geometry_msgs/msg/twist.hpp"

#include <thread>
#include <string>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <memory>
#include <time.h>
#include <signal.h>
#include <atomic>

using namespace std;
using std::placeholders::_1;
using namespace std::chrono_literals;

/********************************************************************************/
std::atomic<bool> stop_requested(false);
void signal_handler(int signal) 
{
    stop_requested = true;
    printf("Signal received: %d. Preparing to shutdown...\n", signal);
}
/********************************************************************************/

class TCPClientNode : public rclcpp::Node
{
public:
    TCPClientNode()
    : Node("tcp_client_node"), sock_(-1)
    {
        this->declare_parameter<std::string>("server_ip", "192.168.123.53");
        this->declare_parameter<int>("server_port", 9000);

        this->get_parameter("server_ip", server_ip_);
        this->get_parameter("server_port", server_port_);

        //publisher list////////////////////////////////////////////////////////////////////////////
        from_ros1_publisher = this->create_publisher<std_msgs::msg::String>("from_ros1", 10);

        cmd_vel_publisher = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel_PI", 10);

        sonic_1_publisher = this->create_publisher<std_msgs::msg::Float64>("sonic_1", 10);
        sonic_2_publisher = this->create_publisher<std_msgs::msg::Float64>("sonic_2", 10);
        sonic_3_publisher = this->create_publisher<std_msgs::msg::Float64>("sonic_3", 10);
        sonic_4_publisher = this->create_publisher<std_msgs::msg::Float64>("sonic_4", 10);

        rader_publisher = this->create_publisher<std_msgs::msg::Float64>("rader", 10);
        lidar_1_publisher = this->create_publisher<std_msgs::msg::Int32>("lidar_1", 1);
        dir_publisher = this->create_publisher<std_msgs::msg::Int32>("dir", 1);

        infrared_1_publisher = this->create_publisher<std_msgs::msg::Float64>("infrared_1", 10);
        infrared_2_publisher = this->create_publisher<std_msgs::msg::Float64>("infrared_2", 10);
        infrared_3_publisher = this->create_publisher<std_msgs::msg::Float64>("infrared_3", 10);
        infrared_4_publisher = this->create_publisher<std_msgs::msg::Float64>("infrared_4", 10);
        infrared_5_publisher = this->create_publisher<std_msgs::msg::Float64>("infrared_5", 10);
        infrared_6_publisher = this->create_publisher<std_msgs::msg::Float64>("infrared_6", 10);
        infrared_7_publisher = this->create_publisher<std_msgs::msg::Float64>("infrared_7", 10);
        infrared_8_publisher = this->create_publisher<std_msgs::msg::Float64>("infrared_8", 10);

        ///////////////////////////////////////////////////////////////////////////////////////////
        connect_to_server();
        recv_thread_ = std::thread(&TCPClientNode::receive_loop, this);
        //Timer
        timer_ = this->create_wall_timer(30ms, std::bind(&TCPClientNode::Timer_function, this));

    }

    rclcpp::TimerBase::SharedPtr timer_;

    ~TCPClientNode()
    {
        stop_requested = true;

        if (recv_thread_.joinable())
            recv_thread_.join();

        if (sock_ != -1)
        {
            close(sock_);
        }
    }

    void Timer_function()
    {
        std::string data = "DS,2,1,STATUS,XX\n";

        if (sock_ >= 0)
        {
            ssize_t sent = send(sock_, data.c_str(), data.length(), MSG_NOSIGNAL);
            if (sent < 0)
            {
                RCLCPP_ERROR(this->get_logger(), "Failed to send message: %s", strerror(errno));
                close(sock_);
                sock_ = -1;
            }
        }
    }

private:
    void connect_to_server()
    {
        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ < 0)
        {
            RCLCPP_ERROR(this->get_logger(), "Socket creation failed");
            return;
        }

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(server_port_);

        if (inet_pton(AF_INET, server_ip_.c_str(), &serv_addr.sin_addr) <= 0)
        {
            RCLCPP_ERROR(this->get_logger(), "Invalid address/Address not supported");
            return;
        }

        if (connect(sock_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            RCLCPP_ERROR(this->get_logger(), "Connection to server failed");
            return;
        }

        RCLCPP_INFO(this->get_logger(), "Connected to TCP server at %s:%d", server_ip_.c_str(), server_port_);
    }

    void reconnect()
    {
        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ < 0)
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to create socket: %s", strerror(errno));
            return;
        }

        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(server_port_);
        inet_pton(AF_INET, server_ip_.c_str(), &serv_addr.sin_addr);

        int result = connect(sock_, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
        if (result == 0)
        {
            RCLCPP_INFO(this->get_logger(), "Reconnected to server.");
        }
        else
        {
            RCLCPP_WARN(this->get_logger(), "Reconnect failed: %s", strerror(errno));
            close(sock_);
            sock_ = -1;
        }
    }


    void receive_loop()
    {
        char buffer[1024];
        std::string rx_buffer;

        while (!stop_requested)
        {
            if (sock_ < 0) 
            {
                RCLCPP_WARN(this->get_logger(), "Socket not connected. Retrying in 1 sec...");
                reconnect(); //try reconnect...
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            ssize_t len = recv(sock_, buffer, sizeof(buffer) - 1, 0);
            if (len > 0)
            {
                buffer[len] = '\0';
                rx_buffer.append(buffer, len);

                size_t pos;
                while ((pos = rx_buffer.find('\n')) != std::string::npos)
                {
                    std::string packet = rx_buffer.substr(0, pos);
                    rx_buffer.erase(0, pos + 1);

                    // ',' parsing
                    std::vector<std::string> tokens;
                    std::stringstream ss(packet);
                    std::string item;
                    while (std::getline(ss, item, ',')) //toral item 21
                    {
                        tokens.push_back(item);
                    }

                    /**********************************************************************************/
                    //cmd_vel_PI
                    auto msg3 = geometry_msgs::msg::Twist();
                    msg3.linear.x = atof(tokens[3].c_str());
                    msg3.angular.z = 0.0;
                    cmd_vel_publisher->publish(msg3);

                    //sonic_1
                    auto msg4 = std_msgs::msg::Float64();
                    msg4.data = atof(tokens[4].c_str());
                    sonic_1_publisher->publish(msg4);
                    //sonic_2
                    auto msg5 = std_msgs::msg::Float64();
                    msg5.data = atof(tokens[5].c_str());
                    sonic_2_publisher->publish(msg5);
                    //sonic_3
                    auto msg6 = std_msgs::msg::Float64();
                    msg6.data = atof(tokens[6].c_str());
                    sonic_3_publisher->publish(msg6);
                    //sonic_4
                    auto msg7 = std_msgs::msg::Float64();
                    msg7.data = atof(tokens[7].c_str());
                    sonic_4_publisher->publish(msg7);

                    //rader
                    auto msg8 = std_msgs::msg::Float64();
                    msg8.data = atof(tokens[7].c_str());
                    rader_publisher->publish(msg8);
                    //lidar_1
                    auto msg9 = std_msgs::msg::Int32();
                    msg9.data = atoi(tokens[8].c_str());
                    lidar_1_publisher->publish(msg9);
                    //dir
                    auto msg10 = std_msgs::msg::Int32();
                    msg10.data = atoi(tokens[9].c_str());
                    dir_publisher->publish(msg10);

                    //infrared_1
                    auto msg11 = std_msgs::msg::Float64();
                    msg11.data = atof(tokens[10].c_str());
                    infrared_1_publisher->publish(msg11);
                    //infrared_2
                    auto msg12 = std_msgs::msg::Float64();
                    msg12.data = atof(tokens[11].c_str());
                    infrared_2_publisher->publish(msg12);
                    //infrared_3
                    auto msg13 = std_msgs::msg::Float64();
                    msg13.data = atof(tokens[12].c_str());
                    infrared_3_publisher->publish(msg13);
                    //infrared_4
                    auto msg14 = std_msgs::msg::Float64();
                    msg14.data = atof(tokens[13].c_str());
                    infrared_4_publisher->publish(msg14);
                    //infrared_5
                    auto msg15 = std_msgs::msg::Float64();
                    msg15.data = atof(tokens[14].c_str());
                    infrared_5_publisher->publish(msg15);
                    //infrared_6
                    auto msg16 = std_msgs::msg::Float64();
                    msg16.data = atof(tokens[15].c_str());
                    infrared_6_publisher->publish(msg16);
                    //infrared_7
                    auto msg17 = std_msgs::msg::Float64();
                    msg17.data = atof(tokens[16].c_str());
                    infrared_7_publisher->publish(msg17);
                    //infrared_8
                    auto msg18 = std_msgs::msg::Float64();
                    msg18.data = atof(tokens[17].c_str());
                    infrared_8_publisher->publish(msg18);

                    /**********************************************************************************/

                    // pub topic
                    if (!tokens.empty()) {
                        auto msg = std_msgs::msg::String();
                        msg.data = packet;
                        from_ros1_publisher->publish(msg);
                    }

                    // RCLCPP_INFO(this->get_logger(), "Parsed packet: %s", packet.c_str());
                }
            }
            else if (len == 0)
            {
                RCLCPP_WARN(this->get_logger(), "TCP connection closed by server.");
                //break;
                close(sock_);
                sock_ = -1;
            }
            else
            {
                RCLCPP_ERROR(this->get_logger(), "TCP receive error");
                //break;
                close(sock_);
                sock_ = -1;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    std::string server_ip_;
    int server_port_;
    int sock_;

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr from_ros1_publisher;

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr sonic_1_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr sonic_2_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr sonic_3_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr sonic_4_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr rader_publisher;
    rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr lidar_1_publisher;
    rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr dir_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr infrared_1_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr infrared_2_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr infrared_3_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr infrared_4_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr infrared_5_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr infrared_6_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr infrared_7_publisher;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr infrared_8_publisher;

    std::thread recv_thread_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    std::signal(SIGINT, signal_handler);
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    auto node = std::make_shared<TCPClientNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
