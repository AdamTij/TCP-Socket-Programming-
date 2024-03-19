/*
    Adamou Tidjani
    CPSC3500 
    PA4: Treasure Hunt/Server code
    03/11/24
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <cerrno>
#include <random>
#include <cmath>
#include <vector>
#include <algorithm>
#include <climits>

using namespace std;

pthread_mutex_t mutex; // lock for critical sections

struct Message // struct to store received messages
{
    char *data;    // pointer to message data
    size_t length; // length of the message
};

// prints appropriate error
void printError(string action, string object)
{
    cout << "Failure to " << action << " " << object << endl;
}

// send a message to client
bool sendMessage(int clientSock, const char *message)
{
    int bytesSent = send(clientSock, (void *)message, strlen(message), 0);

    return bytesSent == strlen(message);
}

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

struct leaderBoard
{
    vector<player> players;
};

leaderBoard board; // leaderboard to store top 3 players

struct treasureLocation // store treasure location
{
    long x;
    long y;

    // Parameterized constructor to initialize location with x and y
    treasureLocation(long x, long y)
    {
        this->x = x;
        this->y = y;
    }
};

// receive integer from client
long receiveInt(int clientSock)
{
    int bytesLeft = sizeof(long);
    long networkInt;

    // char * used because char is 1 byte
    char *bp = (char *)&networkInt;

    while (bytesLeft)
    {
        int bytesRecv = recv(clientSock, bp, bytesLeft, 0);

        if (bytesRecv <= 0)
        {
            // return a value for failure
            return LONG_MIN;
        }

        bytesLeft -= bytesRecv;
        bp += bytesRecv;
    }

    // convert int to host order before returning it
    int hostInt = ntohl(networkInt);

    return hostInt;
};

// receive message from client
Message receiveMessage(int clientSock, int hostInt)
{
    int msgLen = hostInt;
    char *buffer = new char[msgLen];
    char *bp = buffer;
    Message result;

    while (msgLen > 0)
    {
        int bytesRecv = recv(clientSock, bp, msgLen, 0);
        if (bytesRecv <= 0)
        {
            // Free the allocated memory
            delete[] buffer;

            // Return an "empty" message for failure
            result.data = nullptr;
            result.length = 0;
            return result;
        }
        msgLen -= bytesRecv;
        bp += bytesRecv;
    }

    // return received message
    result.data = buffer;
    result.length = hostInt;

    return result;
};

// send int to client
bool sendInt(int sock, long hostInt)
{
    // convert int to network order before sending
    long networkInt = htonl(hostInt);
    long bytesSent = send(sock, (void *)&networkInt, sizeof(long), 0);

    return bytesSent == sizeof(long);
}

// send distance to the treasure to client
bool sendDistance(int sock, double hostDouble)
{
    // send the double to the specified socket
    double bytesSent = send(sock, (void *)&hostDouble, sizeof(double), 0);

    return bytesSent == sizeof(double);
}

// calculate distance between treasure location and user guess
double calcDist(long randomX, long randomY, long userX, long userY)
{
    return sqrt(pow((randomX - userX), 2) + pow((randomY - userY), 2));
}

// generate random int
long generateLong()
{
    // Use a random_device to seed the random number generator
    random_device rd;

    // Use the random_device to seed the random engine
    mt19937 gen(rd());

    // Define the distribution for the range -100 to 100 (inclusive)
    uniform_int_distribution<long> distribution(-100, 100);

    // Generate a random number
    long randomNumber = distribution(gen);

    return randomNumber;
}

// update leaderboard
void updateBoard(leaderBoard &board, const player newPlayer)
{
    // Add the new player to the leaderboard
    board.players.push_back(newPlayer);

    // Sort the players based on their number of tries in ascending order
    sort(board.players.begin(), board.players.end(), [](const player &a, 
    const player &b){ return a.tries < b.tries; });

    // Keep only the top 3 players
    if (board.players.size() > 3)
    {
        board.players.resize(3);
    }
}

// play the game
void playGame(int clientSock)
{
    // initialize lock
    pthread_mutex_init(&mutex, NULL);

    // Send welcome message to the client
    const char *message = "Welcome to Treasure Hunt\nEnter your name: ";

    // length
    if (!sendInt(clientSock, strlen(message)))
    {
        printError("send", "welcome message length");
        return;
    }

    // message
    if (!sendMessage(clientSock, message))
    {
        printError("send", "welcome message");
        return;
    }

    long usrnameLength = receiveInt(clientSock);
    if (usrnameLength == LONG_MIN)
    {
        // print an error message and close connection with client
        printError("receive", "username length");
        return;
    }

    // receive username
    Message msg = receiveMessage(clientSock, usrnameLength);

    if (msg.data == nullptr)
    {
        printError("receive", "username");
        return;
    }

    // initialize a new player
    player newPlayer = player(msg.data, 0);

    // generate random location
    long randomX = generateLong();
    long randomY = generateLong();
    treasureLocation location = treasureLocation(randomX, randomY);

    const char *playMsg = "Enter a guess (x y) : ";

    long userX;
    long userY;

    // print treasure location on the server console
    cout << "Tresure is located at (" << randomX << ", " << randomY << ")\n";

    do
    { // keep playing until user leaves or guess is correct

        newPlayer.tries++;

        // send "Turn (tries)" meassage to client
        string turn = "\nTurn: " + to_string(newPlayer.tries) + "\n";
        char *trn = new char[turn.length()];
        strcpy(trn, turn.c_str());

        if (!sendInt(clientSock, strlen(trn)))
        {
            printError("send", "number of turn(s) length");
            return;
        };

        if (!sendMessage(clientSock, trn))
        {
            printError("send", "number of turns");
            return;
        };

        // Ask user for a guess
        if (!sendInt(clientSock, strlen(playMsg)))
        {
            printError("send", "\"enter guess\" length");
            return;
        };

        if (!sendMessage(clientSock, playMsg))
        {
            printError("send", "\"enter guess\"");
            return;
        };

        /* receive user guess
        print error message and abort if guess not received*/
        userX = receiveInt(clientSock);
        if (userX == LONG_MIN)
        {
            printError("receive", "user guess");
            return;
        }

        userY = receiveInt(clientSock);
        if (userX == LONG_MIN)
        {
            printError("receive", "user guess");
            return;
        }

        // calculate and send distance to treasure
        double distance = calcDist(randomX, randomY, userX, userY);
        if(!sendDistance(clientSock, distance)){
            printError("send","distance to treasure location");
            return;
        };

        // free memory
        delete[] trn;

    } while ((randomX != userX) || (randomY != userY));

    // locks before entering critical section
    pthread_mutex_lock(&mutex);
    updateBoard(board, newPlayer);
    pthread_mutex_unlock(&mutex); // unlocks after critical section

    string cngratMsg =
        "Congratulations! You found the treasure!\nIt took " +
        to_string(newPlayer.tries) +
        (newPlayer.tries == 1 ? " turn" : " turns") +
        " to find the treasure.";

    // send congratulation message
    char *congratMsg = new char[cngratMsg.length()];
    strcpy(congratMsg, cngratMsg.c_str());

    if (!sendInt(clientSock, strlen(congratMsg)))
    {
        printError("send","congratulation message length");
        delete[] congratMsg; 
        return;
    };

    if (!sendMessage(clientSock, congratMsg))
    {
        printError("send","congratulation message");
        delete[] congratMsg; 
        return;
    };

    // send leaderboard size
    long boardSize = board.players.size();
    if(!sendInt(clientSock, boardSize)){

        printError("send","leaderboard size");
        delete[] congratMsg; 
        return;
    };

    // send leaderboard info
    for (int i = 0; i < boardSize; i++)
    {
        string Name = board.players[i].name;
        char *name = new char[board.players[i].name.length()];
        strcpy(name, Name.c_str());

        // name
        if (!sendInt(clientSock, strlen(name)))
        {
            printError("send","player name length");
            delete[] name;
            delete[] congratMsg; 
            return;
        }

        if (!sendMessage(clientSock, name))
        {
            printError("send","player name");
            delete[] name;
            delete[] congratMsg; 
            return;
        }

        // tries
        if (!sendInt(clientSock, board.players[i].tries))
        {
            printError("send","player tries");
            delete[] name; 
            delete[] congratMsg; 
            return;
        }

        delete[] name;
    }

    // free ressources
    delete[] congratMsg;
    pthread_mutex_destroy(&mutex);

    return;
}

// arguments for thread function
struct ThreadArgs
{
    int clientSock;
};

// thread argument function
void *threadMain(void *args)
{
    // Extract socket file descriptor from argument
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)args;
    int clientSock = threadArgs->clientSock;
    delete threadArgs;

    // Communicate with client
    playGame(clientSock);

    // Reclaim ressources before finishing
    pthread_detach(pthread_self());
    close(clientSock);

    return NULL;
}

int main(int argc, char **argv)
{

    if (argc != 2)
    {
        // check if all arguments are provided
        cerr << "Usage: ./pa4_server [port number]" << endl;
        exit(EXIT_FAILURE);
    }

    // create a TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock < 0)
    {
        cerr << "Error creating server socket" << endl;
        exit(EXIT_FAILURE);
    }

    // read port number from command line
    unsigned short servPort = (short)stoi(argv[1]);

    // set the fields
    struct sockaddr_in servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(servPort);

    // bind port to socket
    int status = bind(sock, (struct sockaddr *)&servAddr, sizeof(servAddr));

    if (status < 0)
    {
        // print out an error message if unsuccessful
        cerr << "Error with bind" << endl;
        close(sock);
        exit(EXIT_FAILURE);
    }

    // set socket to listen
    status = listen(sock, 5);

    if (status < 0)
    {
        cerr << "Error with listen" << endl;
        close(sock);
        exit(EXIT_FAILURE);
    }

    while (true)
    {
        int clientSock;

        // Accept connection from client
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        clientSock = accept(sock, (struct sockaddr *)&clientAddr, &addrLen);

        if (clientSock < 0)
        {
            cerr << "Error with accept" << endl;
        }

        // Create and initialize argument struct
        ThreadArgs *args = new ThreadArgs;
        args->clientSock = clientSock;

        // Create Thread
        pthread_t threadID;

        // let client play
        int status = pthread_create(&threadID, NULL, threadMain, (void *)args);
        if (status != 0)
        {
            cerr << "Error creating thread " << threadID << endl;
            exit(EXIT_FAILURE);
        }
    }

    // Close sockets when done
    close(sock);
}