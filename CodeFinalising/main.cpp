#include <enet/enet.h>
#include <string>
#include <iostream>
#include <thread>
#include <conio.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Winmm.lib")

using namespace std;

ENetHost* NetHost = nullptr;
ENetPeer* Peer = nullptr;
bool IsServer = false;
thread* PacketThread = nullptr;
bool GameDone = false;
int GuessingNumber = 0;

enum PacketHeaderTypes
{
    PHT_Invalid = 0,
    PHT_IsDead,
    PHT_Position,
    PHT_Count,
    PHT_GameIntro,
    PHT_Guessing,
    PHT_CorrectGuess,
    PHT_Waiting,
    PHT_GameOver
};

struct GamePacket
{
    GamePacket() {}
    PacketHeaderTypes Type = PHT_Invalid;
};

struct IsDeadPacket : public GamePacket
{
    IsDeadPacket()
    {
        Type = PHT_IsDead;
    }

    int playerId = 0;
    bool IsDead = false;
};

struct GuessWasCorrectPacket : public GamePacket
{
    GuessWasCorrectPacket()
    {
        Type = PHT_CorrectGuess;
    }
    string message = "";
    int playerId = 0;
};

struct GameIntroPacket : public GamePacket
{
    GameIntroPacket()
    {
        Type = PHT_GameIntro;
    }

    int playerId = 0;
    bool IsDead = false;
};

struct GuessingPacket : public GamePacket
{
    GuessingPacket()
    {
        Type = PHT_Guessing;
    }
    int guessingNumber = 0;
    ENetPeer* peer = nullptr;
};

struct WaitingForPlayersPacket : public GamePacket
{
    WaitingForPlayersPacket()
    {
        Type = PHT_Waiting;
    }
    int playerId = 0;
};

struct PositionPacket : public GamePacket
{
    PositionPacket()
    {
        Type = PHT_Position;
    }

    int playerId = 0;
    int x = 0;
    int y = 0;
};

struct GameOverPacket : public GamePacket
{
    GameOverPacket()
    {
        Type = PHT_GameOver;
    }

    string message = "";

};

//can pass in a peer connection if wanting to limit
bool CreateServer()
{
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 1234;
    NetHost = enet_host_create(&address /* the address to bind the server host to */,
        32      /* allow up to 32 clients and/or outgoing connections */,
        2      /* allow up to 2 channels to be used, 0 and 1 */,
        0      /* assume any amount of incoming bandwidth */,
        0      /* assume any amount of outgoing bandwidth */);

    return NetHost != nullptr;
}

bool CreateClient()
{
    NetHost = enet_host_create(NULL /* create a client host */,
        1 /* only allow 1 outgoing connection */,
        2 /* allow up 2 channels to be used, 0 and 1 */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);

    return NetHost != nullptr;
}

bool AttemptConnectToServer()
{
    ENetAddress address;
    /* Connect to some.server.net:1234. */
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 1234;
    /* Initiate the connection, allocating the two channels 0 and 1. */
    Peer = enet_host_connect(NetHost, &address, 2, 0);
    return Peer != nullptr;
}

void SendCorrectGuessPacket(ENetPeer* peer)
{
    GuessWasCorrectPacket* CorrectGuessPacket = new GuessWasCorrectPacket();
    CorrectGuessPacket->message = "YOU ARE THE WINNER AND GUESSED CORRECTLY!";
    ENetPacket* packet = enet_packet_create(CorrectGuessPacket,
        sizeof(CorrectGuessPacket),
        ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(NetHost);
    delete CorrectGuessPacket;
}

void HandleReceivePacket(const ENetEvent& event)
{
    GamePacket* RecGamePacket = (GamePacket*)(event.packet->data);
    if (RecGamePacket)
    {
        cout << "Received Game Packet " << endl;

        if (RecGamePacket->Type == PHT_IsDead)
        {
            cout << "u dead?" << endl;
            IsDeadPacket* DeadGamePacket = (IsDeadPacket*)(event.packet->data);
            if (DeadGamePacket)
            {
                string response = (DeadGamePacket->IsDead ? "yeah" : "no");
                cout << response << endl;
            }
        }

        if (RecGamePacket->Type == PHT_GameIntro)
        {
            cout << "Welcome to the guessing game!" << endl;
            cout << "Guess any number between 1 and 10." << endl;
            cout << "The player who guesses correctly wins!" << endl;
        }

        if (RecGamePacket->Type == PHT_Guessing)
        {
            GuessingPacket* guessPacket = (GuessingPacket*)(event.packet->data);
            if (guessPacket)
            {
                if (guessPacket->guessingNumber == GuessingNumber)
                {
                    GameDone = true;
                    SendCorrectGuessPacket(event.peer);
                }
                else
                {
                    //Send IncorrectGuessPacket()
                }
            }
        }

        if (RecGamePacket->Type == PHT_Waiting)
        {
            WaitingForPlayersPacket* waitingPacket = (WaitingForPlayersPacket*)(event.packet->data);
            if (waitingPacket)
            {
                cout << "Waiting for more players to start the game." << endl;
            }
        }
        if (RecGamePacket->Type == PHT_CorrectGuess)
        {
            GuessWasCorrectPacket* correctGuessPacket = (GuessWasCorrectPacket*)(event.packet->data);
            if (correctGuessPacket)
            {
                cout << "YOU ARE THE WINNER AND GUESSED CORRECTLY!" << endl;
            }
        }
        if (RecGamePacket->Type == PHT_GameOver)
        {
            GameOverPacket* gameDonePacket = (GameOverPacket*)(event.packet->data);
            if (gameDonePacket)
            {
                cout << "Game Over! The number has been guessed correctly" << endl;
                GameDone = true;
            }
        }
    }
    else
    {
        cout << "Invalid Packet " << endl;
    }

    /* Clean up the packet now that we're done using it. */
    enet_packet_destroy(event.packet);
    {
        enet_host_flush(NetHost);
    }
}

//void BroadcastIsDeadPacket(ENetEvent* e)
//{
//    IsDeadPacket* DeadPacket = new IsDeadPacket();
//    DeadPacket->IsDead = true;
//    ENetPacket* packet = enet_packet_create(DeadPacket,
//        sizeof(IsDeadPacket),
//        ENET_PACKET_FLAG_RELIABLE);
//    enet_peer_send(e->peer, 0, packet);
//    enet_host_flush(NetHost);
//    delete DeadPacket;
//}

void BroadcastGameIntro()
{
    GameIntroPacket* IntroPacket = new GameIntroPacket();
    ENetPacket* packet = enet_packet_create(IntroPacket,
      sizeof(IntroPacket),
      ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(NetHost, 0, packet);
    enet_host_flush(NetHost);
    delete IntroPacket;
}

void BroadcastGameOverPacket()
{
    GameOverPacket* GameDonePacket = new GameOverPacket();
    GameDonePacket->message = "Game Over! The number has been guessed correctly and it was the number: " + GuessingNumber;
    ENetPacket* packet = enet_packet_create(GameDonePacket,
        sizeof(GameDonePacket),
        ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(NetHost, 0, packet);
    enet_host_flush(NetHost);
    delete GameDonePacket;
}

void BroadCastWaitingForPlayers(ENetEvent* e)
{
    WaitingForPlayersPacket* WaitPacket = new WaitingForPlayersPacket();
    ENetPacket* packet = enet_packet_create(WaitPacket,
        sizeof(WaitPacket),
        ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(e->peer, 0, packet);
    enet_host_flush(NetHost);
    delete WaitPacket;
}

void ServerProcessPackets()
{
    srand(time(0));
    GuessingNumber = rand() % 10 + 1;
    cout << GuessingNumber;
    int numberOfPlayers = 0;
    bool gameStarted = false;
    bool introBroadcastHasHappened = false;

    while (!GameDone)
    {
        if (gameStarted == true && introBroadcastHasHappened == false)
        {
            introBroadcastHasHappened = true;
            BroadcastGameIntro();
        }
        ENetEvent event;
        while (enet_host_service(NetHost, &event, 1000) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                cout << "A new client connected from "
                    << event.peer->address.host
                    << ":" << event.peer->address.port
                    << endl;
                /* Store any relevant client information here. */
                event.peer->data = (void*)("Client information");
                //BroadcastIsDeadPacket(&event);
                numberOfPlayers++;
                if (numberOfPlayers >= 2) gameStarted = true;
                else BroadCastWaitingForPlayers(&event);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                HandleReceivePacket(event);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                cout << (char*)event.peer->data << "disconnected." << endl;
                event.peer->data = NULL;
                //notify remaining player that the game is done due to player leaving
            }
        }
    }
    //Create A Game Ended Packet
    BroadcastGameOverPacket();
}

void SendMessagesToServer()
{
    while (!GameDone)
    {
        if (_kbhit())
        {
            GuessingPacket* guessPacket = new GuessingPacket();
            int number;
            cin >> number;
            guessPacket->guessingNumber = number;
            guessPacket->peer = Peer;
            ENetPacket* packet = enet_packet_create(guessPacket,
                sizeof(guessPacket),
                ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(Peer, 0, packet);
            enet_host_flush(NetHost);
            delete guessPacket;
        }
    }
}

void ClientProcessPackets()
{
    while (!GameDone)
    {
        ENetEvent event;
        std::thread* ClientToServer = nullptr;
        while (enet_host_service(NetHost, &event, 1000) > 0)
        {
            switch (event.type)
            {
            case  ENET_EVENT_TYPE_CONNECT:
                cout << "Connection succeeded " << endl;
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                HandleReceivePacket(event);
                if (ClientToServer == nullptr)
                {
                    ClientToServer = new std::thread(SendMessagesToServer);
                }
                break;
            }
        }
    }
}

int main(int argc, char** argv)
{
    if (enet_initialize() != 0)
    {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        cout << "An error occurred while initializing ENet." << endl;
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    cout << "1) Create Server " << endl;
    cout << "2) Create Client " << endl;
    int UserInput;
    cin >> UserInput;
    if (UserInput == 1)
    {
        //How many players?

        if (!CreateServer())
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet server.\n");
            exit(EXIT_FAILURE);
        }

        IsServer = true;
        cout << "waiting for players to join..." << endl;
        PacketThread = new thread(ServerProcessPackets);
    }
    else if (UserInput == 2)
    {
        if (!CreateClient())
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet client host.\n");
            exit(EXIT_FAILURE);
        }

        ENetEvent event;
        if (!AttemptConnectToServer())
        {
            fprintf(stderr,
                "No available peers for initiating an ENet connection.\n");
            exit(EXIT_FAILURE);
        }

        PacketThread = new thread(ClientProcessPackets);

        //handle possible connection failure
        {
            //enet_peer_reset(Peer);
            //cout << "Connection to 127.0.0.1:1234 failed." << endl;
        }
    }
    else
    {
        cout << "Invalid Input" << endl;
    }


    if (PacketThread)
    {
        PacketThread->join();
    }
    delete PacketThread;
    if (NetHost != nullptr)
    {
        enet_host_destroy(NetHost);
    }

    return EXIT_SUCCESS;
}