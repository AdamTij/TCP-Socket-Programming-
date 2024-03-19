/*
    Adamou Tidjani
    CPSC3500 
    PA4: Treasure Hunt/Client code
    03/11/24
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <string.h>
#include <cerrno>
#include <vector>
#include <climits>
#include <iomanip>

using namespace std;

struct player
{
    // data fields
    string name;
    int tries;

    // Default constructor
    player() : name(""), tries(0) {}

    // Parameterized constructor to initialize player with name and tries
    player(char *name, int tries)
    {
        this->name = name;
        this->tries = tries;
    }
};

struct Message // struct to store received messages
{
    char *data;    // pointer to message data
    size_t length; // length of the message
};

// receive distance to treasure from server
double receiveDistance(int sock)
{
    double bytesLeft = sizeof(double);
    double hostDouble;
    char *bp = reinterpret_cast<char *>(&hostDouble);

    // receive 1 byte at a time
    while (bytesLeft)
    {
        int bytesRecv = recv(sock, bp, bytesLeft, 0);

        if (bytesRecv <= 0)
        {
            // return error value
            return -1;
        }

        bytesLeft -= bytesRecv;
        bp += bytesRecv; // advance pointer
    }

    return hostDouble;
}

// prints appropriate error
void printError(string action, string object)
{
    cout << "Failure to " << action << " " << object << endl;
}

// receive an integer from server
long receiveInt(int sock)
{
    int bytesLeft = sizeof(long);
    long networkInt;

    // char * used because char is 1 byte
    char *bp = (char *)&networkInt;

    // receive 1 byte at a time
    while (bytesLeft)
    {
        int bytesRecv = recv(sock, bp, bytesLeft, 0);

        if (bytesRecv <= 0)
        {
            // error value
            return LONG_MIN;
        }

        bytesLeft -= bytesRecv;
        bp += bytesRecv; // advance pointer
    }

    // convert to host order before returning it
    long hostInt = ntohl(networkInt);

    return hostInt;
};

// receive a message from server
Message receiveMessage(int sock, int hostInt)
{
    int msgLen = hostInt;
    char *buffer = new char[msgLen];
    char *bp = buffer;
    Message result;

    // Loop until the entire message is received
    while (msgLen > 0)
    {
        int bytesRecv = recv(sock, bp, msgLen, 0);

        // Check for errors or connection closure
        if (bytesRecv <= 0)
        {
            cerr << "Failure to receive a message" << endl;

            // Free the allocated memory
            delete[] buffer;

            // Return an "empty" message for failure
            result.data = nullptr;
            result.length = 0;
            return result;
        }

        // Adjust pointers and remaining length
        msgLen -= bytesRecv;
        bp += bytesRecv;
    }

    // Populate the result struct with received data
    result.data = buffer;
    result.length = hostInt;

    return result;
};

// send an integer to the server
bool sendInt(int sock, long hostInt)
{
    // Convert the long integer to network order
    long networkInt = htonl(hostInt);

    // Send the network-ordered integer to the specified socket
    long bytesSent = send(sock, (void *)&networkInt, sizeof(long), 0);

    // Check if the entire message was sent successfully
    return bytesSent == sizeof(long);
}

// send a message to the server
bool sendMessage(int clientSock, const char *message)
{
    int bytesSent = send(clientSock, (void *)message, strlen(message), 0);

    return bytesSent == strlen(message);
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        // check if all arguments are provided
        cerr << "Usage: " << argv[0] << " [IP address] [port number]" << endl;
        exit(EXIT_FAILURE);
    }

    // create a TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock < 0)
    {
        cerr << "Error creating socket: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }

    // read IP address and port number from command line
    char *IPAddr = argv[1];
    unsigned short servPort = (short)(stoi(argv[2]));

    // Convert dotted decimal address to int
    unsigned long servIP;
    int status = inet_pton(AF_INET, IPAddr, (void *)&servIP);

    if (status == -1)
    {
        cerr << "Unknown family address" << endl;
        exit(EXIT_FAILURE);
    }

    if (status == 0)
    {
        cerr << "Invalid IP address" << endl;
        exit(EXIT_FAILURE);
    }

    // set the fields
    struct sockaddr_in servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = servIP;
    servAddr.sin_port = htons(servPort);

    // establish connection with the server
    status = connect(sock, (struct sockaddr *)(&servAddr), sizeof(servAddr));

    if (status < 0)
    {
        // print out an error message if unsuccessful
        cerr << "Error with connect: " << strerror(errno) << endl;
        close(sock);
        exit(EXIT_FAILURE);
    }

    // receive welcome message
    long msgLength = receiveInt(sock);

    if (msgLength == LONG_MIN)
    {

        printError("receive", "welcome message length");
        close(sock);
        exit(EXIT_FAILURE);
    }

    Message receivedMessage = receiveMessage(sock, msgLength);

    if (receivedMessage.data == nullptr)
    {

        printError("receive", "welcome message");
        close(sock);
        exit(EXIT_FAILURE);
    }
    cout << receivedMessage.data;

    // read username from command line
    string usrname;
    cin >> usrname;

    // send username length
    char *username = new char[usrname.length()];
    strcpy(username, usrname.c_str());
    long usernameNInt = strlen(username);

    if (!sendInt(sock, usernameNInt))
    {
        printError("receive", "username length");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // send actual username
    if (!sendMessage(sock, username))
    {
        printError("send", "username");
        close(sock);
        exit(EXIT_FAILURE);
    }

    delete[] username; // free memory

    long userX;
    long userY;

    do
    { /* keep playing while server is on
      or guess is incorrect */

        // receive number of turn(s)
        msgLength = receiveInt(sock);
        if(msgLength==LONG_MIN){
            printError("receive", "number of turn(s) length");
            break;
        }

        receivedMessage = receiveMessage(sock, msgLength);
        if(receivedMessage.data==nullptr){
            printError("receive", "number of turn(s)");
            break;
        }
        cout << receivedMessage.data;

        // receive "enter your guess" message
        msgLength = receiveInt(sock);
        receivedMessage = receiveMessage(sock, msgLength);
        cout << receivedMessage.data;

        // read guess from command line
        cin >> userX;
        cin >> userY;

        // check for valid input
        while (cin.fail())
        {
            cout << "Invalid input. Try again!\n";
            cout << "Enter a guess (x y) : ";

            // Clear the fail state
            cin.clear();

            // Ignore the invalid input
            cin.ignore(INT_MAX, '\n');

            // Attempt to read user input again
            cin >> userX >> userY;
        }

        // check if guess is within the grid
        while ((-100 > userX || userX > 100) || ((-100 > userY || userY > 100)))
        {
            cout << "Coordinates out of bounds. Try again!" << endl;
            cout << "Enter a guess (x y) : ";

            cin.clear();
            cin.ignore(INT_MAX, '\n');

            // Attempt to read user input again
            cin >> userX >> userY;
        }

        // send valid guesses to server
        sendInt(sock, userX);
        sendInt(sock, userY);

        // receive distance from treasure location
        double distance = receiveDistance(sock);

        if (distance == 0)
        {
            cout << "Distance to treasure: " << distance << " ft.\n\n";
            msgLength = receiveInt(sock);
            if(msgLength==LONG_MIN){
                printError("receive","victory message length");
                break;
            }

            // receive victory messagee
            receivedMessage = receiveMessage(sock, msgLength);
            if(receivedMessage.data==nullptr){
                printError("receive","victory message");
                break;
            }
            cout << receivedMessage.data << endl;

            // receive leaderboard size
            long boardSize = receiveInt(sock);

            // print leaderboard
            cout << "\nLeader board:" << endl;
            for (int i = 0; i < boardSize; i++)
            {
                long nameLength = receiveInt(sock);
                if(nameLength==LONG_MIN){
                    printError("receive","player name length");
                    break;
                }

                Message name = receiveMessage(sock, nameLength);
                if(name.data==nullptr){
                    printError("receive","player name");
                    break;
                }

                long tries = receiveInt(sock);
                if(tries==LONG_MIN){
                    printError("receive","player tries");
                    break;
                }

                cout << (i + 1) << ". " << name.data << " " << tries << endl;
            }

            break;
        }

        // else just print distance and start over
        cout << "Distance to treasure: " << fixed << setprecision(2)
             << distance << " ft.\n";

    } while (true);

    // close connection when done
    close(sock);

    return 0;
}