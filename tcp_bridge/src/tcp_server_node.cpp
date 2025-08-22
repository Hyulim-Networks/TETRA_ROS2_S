
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

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

/********************************************************************************/
std::atomic<bool> stop_requested(false);
void signal_handler(int signal) 
{
    stop_requested = true;
    printf("Signal received: %d. Preparing to shutdown...\n", signal);
}
/********************************************************************************/

class TCPServerNode : public rclcpp::Node
{
public:
    TCPServerNode()
    : Node("tcp_server_node")
    {
        publisher_ = this->create_publisher<std_msgs::msg::String>("from_ros1", 10);
        RCLCPP_INFO(this->get_logger(), "Starting TCP server...");
        server_thread_ = std::thread(std::bind(&TCPServerNode::start_server, this));
    }

    ~TCPServerNode()
    {
        if (server_thread_.joinable())
            server_thread_.join();
    }

private:
    void start_server()
    {
        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);
        char buffer[1024] = {0};

        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
            RCLCPP_ERROR(this->get_logger(), "Socket creation failed");
            return;
        }

        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(9000);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            RCLCPP_ERROR(this->get_logger(), "Bind failed");
            return;
        }

        if (listen(server_fd, 3) < 0)
        {
            RCLCPP_ERROR(this->get_logger(), "Listen failed");
            return;
        }

        RCLCPP_INFO(this->get_logger(), "TCP Server listening on port 9000...");

        while (rclcpp::ok())
        {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (new_socket < 0)
            {
                RCLCPP_ERROR(this->get_logger(), "Accept failed");
                continue;
            }

            RCLCPP_INFO(this->get_logger(), "Client connected");

            int valread;
            while ((valread = read(new_socket, buffer, 1024)) > 0)
            {
                std_msgs::msg::String msg;
                msg.data = std::string(buffer, valread);
                publisher_->publish(msg);
                RCLCPP_INFO(this->get_logger(), "Received and published: %s", msg.data.c_str());
            }

            close(new_socket);
            RCLCPP_INFO(this->get_logger(), "Client disconnected");
        }

        close(server_fd);
    }

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    std::thread server_thread_;
};
 
int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    std::signal(SIGINT, signal_handler);
	// Create a function for when messages are to be sent.
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    auto node = std::make_shared<TCPServerNode>();
    
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
