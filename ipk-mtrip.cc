/**
 *  @file       ipk-mtrip.cc
 *  @author     Andrej Nano (xnanoa00)
 *  @date       2018-04-09
 *  @version    1.0
 * 
 *  @brief IPK 2018, 2nd project - Bandwidth Measurement (Ryšavý). MTrip Main source file.
 *  
 *  @section Description
 *  
 *  This program is a school project for Computer Communications and Networks 
 *  course at Faculty of Information Technology of Brno University of 
 *  Technology. Main goal of the project is to use a proper communication to
 *  measure the bandwidth between 2 hosts on the Internet. Use of UDP protocol 
 *  is required for measurment.
 *  
 *  The project consists of 2 parts. A reflector and a meter.
 *  Both are just runtime modes for the same executable.
 *  Measurement will be done by sending UDP packets from the
 *  meter to the reflector, reflector will then respond.
 *  The meter must implement prober algorithm to measure the max
 *  bandwidth at which there are no lost packets.
 * 
 *  @section Usage
 *  
 *  * ./ipk-mtrip reflect -p port 
 *  * ./ipk-mtrip meter -h vzdáleny_host -p vzdálený_port - s velikost_sondy -t doba_mereni
 *  
 */

// std libraries
#include <iostream>
#include <string>
#include <unistd.h>
#include <csignal>
#include <sys/wait.h>

// commonly used std objects.. really no need to be careful about poluting namespace
using std::cout;
using std::cerr;
using std::endl;
using std::string;

// sockets API + networking libraries
#include <sys/socket.h>
#include <netdb.h>              

// mtrip configurations + control/argument parse/interrupt handling
#include "ipk-mtrip.h"

// socket abstraction
#include "ipk-socket.h"


/**
 *  @brief Main entry point, handles common routine and then delegates to runtime modes. 
 *  
 *  @param argc number of string arguments pointed to by argv
 *  @param argv vector of string arguments passed to the program
 *  @return exit code as an int
 */
int main(int argc, char **argv)
{
  // function to handle interrupt
  signal(SIGINT, interrupt_handler);
  
  // create new runtime mtrip configuration
  std::unique_ptr<MTripConfiguration> mtrip = argument_parser(argc, argv);
  
  if(!mtrip)
    return EXIT_FAILURE;

  // ENTRY POINT
  mtrip->init();

  return EXIT_SUCCESS;
}


/**
 * @brief Main routine of the reflector
 * 
 * @desc Initializes the reflector mode routine 
 * as the active configuration
 */
void Reflector::init()
{
  cout << "[REFLECTOR]: " << CL_GREEN << "started" << RESET << endl;

  // create new socket object
  std::unique_ptr<SocketEntity> socket = std::make_unique<SocketEntity>();

  // prepare the server
  socket->setup_server(m_port);

  char buffer[512];

  while (true)
  {
    socket->recv_message(buffer, sizeof(buffer));
    socket->send_message(buffer, sizeof(buffer));
  }
}


/**
 * @brief Main routine of the meter
 * 
 * @desc Initializes the measurement mode routine 
 * as the active configuration
 */
void Meter::init()
{
  cout << "[METER]: " << CL_GREEN << "started" << RESET << endl;

  // create new socket object
  std::unique_ptr<SocketEntity> socket = std::make_unique<SocketEntity>();

  // prepare the remote address
  socket->setup_connection(m_host_name.c_str(), m_port);

  // create specified size probe
  char buffer[m_probe_size];

  while (true)
  {
    bzero(buffer, m_probe_size);
    printf("Please enter msg: ");
    fgets(buffer, m_probe_size, stdin);

    socket->send_message(buffer, sizeof(buffer));

    bzero(buffer, m_probe_size);

    socket->recv_message(buffer, sizeof(buffer));

    cout << "Feedback: " << buffer << endl;
  }

  (void)m_measurment_time;
}


/**
 *  @brief Properly handles interrupt, such as CTRL+C
 *  @param signum number of the signal caught
 *  @return void
 */
void interrupt_handler(int signum)
{
  cout << "Caught signal " << signum << ". Ending the program." << endl;
  exit(EXIT_SUCCESS);
}


/**
 *  @brief Parses arguments, checks their validity and returns a new MTrip Configuration object
 * 
 *  @param argc number of string arguments pointed to by argv
 *  @param argv vector of string arguments passed to the program
 *  @return program runtime configuration distinct for each mode
 */
std::unique_ptr<MTripConfiguration> argument_parser(int argc, char **argv)
{
  if (argc < 4)
  {
    cerr << "Wrong number of arguments." << endl;
    return nullptr;
  }

  char c; // help
  opterr = 0; // turn off getopt errors

  // METER MODE
  if (string(argv[optind]) == "meter")
  {
    optind++;

    // argument options
    bool h_flag = false, p_flag = false, s_flag = false, t_flag = false;
    
    // argument values
    string host_name;
    unsigned short port;
    size_t probe_size;
    float measurment_time;

    while ((c = getopt(argc, argv, "h:p:s:t:")) != -1)
    {
      switch (c)
      {
        case 'h':
          h_flag = true;
          host_name = optarg;
          break;
        case 'p':
          p_flag = true;
          port = static_cast<unsigned int>(atoi(optarg));
          break;
        case 's':
          s_flag = true;
          probe_size = static_cast<size_t>(atoi(optarg));
          break;
        case 't':
          t_flag = true;
          measurment_time = atof(optarg);
          break;
        case '?':
          if (optopt == 'h' || optopt == 'p' || optopt == 's' || optopt == 't')
              cerr << "Option -" << static_cast<char>(optopt) << " requires an argument." << endl;
          else if (isprint(optopt))
              cerr << "Uknown option '-" << static_cast<char>(optopt) << "'" << endl;
          else
              cerr << "Unknown option character. " << endl;
          exit(1);
        default:
          cerr << "uknown getopt() error" << endl;
          exit(1);
          break;
      }
    }

    // everything OK -> create new configuration
    if (h_flag && p_flag && s_flag && t_flag)
    {
      return std::make_unique<Meter>(host_name, port, probe_size, measurment_time);
    }
    else
    {
      cerr << "Not all argument options passed in." << endl;
      return nullptr;
    }
  }
  else
  // REFLECT MODE
  if (string(argv[optind]) == "reflect")
  {
    optind++;

    // argument option + value
    bool p_flag = false;
    unsigned int port;

    while ((c = getopt(argc, argv, "p:")) != -1)
    {
      switch (c)
      {
        case 'p':
          p_flag = true;
          port = static_cast<unsigned int>(atoi(optarg));
          break;
        case '?':
          if (optopt == 'p')
              cerr << "Option -p requires an argument." << endl;
          else if (isprint(optopt))
              cerr << "Uknown option '-" << static_cast<char>(optopt) << "'" << endl;
          else
              cerr << "Unknown option character. " << endl;
          exit(1);
        default:
          cerr << "uknown getopt() error" << endl;
          exit(1);
          break;
      }
    }
    
    // everything OK -> create new configuration
    if (p_flag)
    {
      return std::make_unique<Reflector>(port);
    }
    else
    {
      cerr << "Required option not passed in." << endl;
      return nullptr;
    }
  }
  else
  {
    cerr << "Undefined mode inside an argument passed to the application." << std::endl;
    return nullptr;
  }
  
}
