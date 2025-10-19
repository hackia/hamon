#pragma once

#include "HamonCube.hpp"
#include <map>
#include <string>
#include <vector>

namespace Dualys {
    using WordCountMap = std::map<std::string, int>;

    class HamonNode {
    public:
        /**
         * @brief Build the node's adjacency (neighbors for each node).
         * Populates the node's container with 8 nodes and assigns neighbors such that
         * each pair of connected nodes differs by exactly one bit in their ID.
         */
        void initializeTopology();

        /**
         * @brief Construct a HamonNode with its topology and configuration.
         * @param p_topology_node The Node instance representing this node's topology.
         * @param p_cube The HamonCube instance representing the overall hypercube structure.
         * @param p_configs A vector of NodeConfig instances containing configuration details for all nodes
         */
        HamonNode(Node p_topology_node, HamonCube p_cube, const std::vector<NodeConfig> &p_configs);

        /**
         * @brief Print the final word count results to the console.
         */
        void print_final_results() const;

        /**
         * @brief Close the server socket.
         * @return true if the socket was closed successfully, false otherwise.
         */
        [[nodiscard]] bool close_server_socket() const;

        /**
         * @brief Send a string over a socket.
         * @param sock The socket file descriptor to send the string through.
         * @param str The string to send.
         * @note This function handles sending the length of the string first, followed by the string data itself.
         */
        static void send_string(int sock, const std::string &str);

        /**
         * @brief Run the node's main operations: setup server, distribute tasks, perform map and reduce.
         * @return true if all operations were successful, false otherwise.
         */
        bool run();

        /**
         * @brief Serialize a WordCountMap to a string.
         * @param target_map The WordCountMap to serialize.
         * @return A string representation of the WordCountMap.
         */
        static std::string serialize_map(const WordCountMap &target_map);

        /**
         * @brief Deserialize a string to a WordCountMap and merge it with an existing map.
         * @param x The string representation of the WordCountMap to deserialize.
         * @param map The existing WordCountMap to merge with the deserialized data.
         * @note This function updates the existing map by adding counts from the deserialized map.
         * @warning This function does not perform any error checking on the input string.
         */
        static void deserialize_and_merge_map(const std::string &x, WordCountMap &map);

    private:
        /**
         * @brief Perform the word count task on a given text chunk.
         * @param text_chunk The chunk of text to process.
         * @return A WordCountMap containing the word counts from the text chunk.
         * @note This function splits the text chunk into words based on whitespace and counts occurrences of each word.
         */
        [[nodiscard]] WordCountMap perform_word_count_task(const std::string &text_chunk) const;

        /**
         * @brief Set up the server socket for incoming connections.
         * @return true if the server was set up successfully, false otherwise.
         * @note This function creates a socket, binds it to the specified port, and starts listening for connections.
         * @warning This function does not handle multiple connections or errors in detail.
         */
        bool setup_server();

        /**
         * @brief Distribute text chunks to neighbor nodes and perform the map operation.
         * @return true if the distribution and mapping were successful, false otherwise.
         * @note This function sends text chunks to neighbor nodes, receives their word count results,
         *       and combines them with the local word count results.
         */
        bool distribute_and_map();

        /**
         * @brief Receive a string from a socket.
         * @param client_socket The socket file descriptor to receive the string from.
         * @return The received string.
         */
        static std::string receive_string(int client_socket);

        /**
         * @brief Perform the reduce operation by aggregating word counts from neighbor nodes.
         * @return true if the reduction was successful, false otherwise.
         * @note This function sends the local word count results to neighbor nodes,
         *       receives their aggregated results, and combines them with the local results.
         */
        [[nodiscard]] bool reduce();

        /**
         * @brief Read the entire content of a file into a string.
         * @param filename The name of the file to read.
         * @return A string containing the file's content.
         * @note This function opens the file, reads its content, and returns it as a
         *       string. If the file cannot be opened, an error message is printed and an empty string is returned.
         */
        Node topology_node;
        /**
         * @brief The HamonCube instance representing the overall hypercube structure.
         * @note This member variable holds the hypercube topology, which includes all nodes and their connections.
         *      It is used to determine neighbor nodes for communication and task distribution.
         * @warning This member variable is immutable after construction and should not be modified.
         */
        HamonCube cube;
        /**
         * @brief Configuration details for all nodes in the hypercube.
         * @note This vector contains NodeConfig instances for each node, including their ID, role
         *     (master or worker), IP address, and port number.
         * @warning This member variable is immutable after construction and should not be modified.
         */
        int server_fd;

        /**
         * @brief The port number on which the node's server listens for incoming connections.
         * @note This port number is specified in the node's configuration and is used to bind
         *      the server socket.
         * @warning Ensure that the port number is not already in use by another application.
         */
        int port;
        /**
         * @brief The IP address of the node.
         * @note This IP address is specified in the node's configuration and is used for
         *     network communication with other nodes.
         */
        std::string ip_address;
        /**
         * @brief Indicates whether this node is the master node.
         * @note The master node is responsible for distributing tasks and coordinating the overall operation.
         *     Worker nodes perform the assigned tasks and report results back to the master.
         */
        bool is_master;
        std::string input_file;
        /**
         * @brief The local word count results for this node.
         * @note This map stores the word counts computed by this node during the map phase.
         *      It is used in the reduce phase to aggregate results from neighbor nodes.
         */
        WordCountMap local_counts;
        /**
         * @brief The final aggregated word count results after the reduce phase.
         * @note This map stores the combined word counts from this node and its neighbors.
         *     It is printed to the console at the end of the operation.
         * @warning This map is only valid after the reduce phase has completed.
         */
        std::vector<NodeConfig> all_configs;
    };
}
