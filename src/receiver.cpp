#include <cstdlib>
#include <ccrtp/rtp.h>
#include <fstream>
#include <unordered_map>
#include <thread>
#include "stream_player.h"

using namespace ost;
using namespace std;

class Listener: RTPSession
{
public:
    Listener(InetMcastAddress& ima, tpport_t port) :
            RTPSession(ima,port) {
    }

    Listener(InetHostAddress& ia, tpport_t port) :
            RTPSession(ia,port) {

    }

    void listen() {
        cout << "My SSRC identifier is: "
        << hex << (int)getLocalSSRC() << endl;

        defaultApplication().setSDESItem(SDESItemTypeTOOL,
                                         "rtplisten demo app.");
        setExpireTimeout(1000000);

        setPayloadFormat(StaticPayloadFormat(sptPCMU));
        startRunning();
        for (;;) {
            const AppDataUnit* adu;
            while ( (adu = getData(getFirstTimestamp())) ) {
                cerr << "I got an app. data unit - "
                << adu->getSize()
                << " payload octets ("
                << "pt " << (int)adu->getType()
                << ") from "
                << hex << (int)adu->getSource().getID()
                << "@" << dec <<
                adu->getSource().getNetworkAddress()
                << ":"
                << adu->getSource().getDataTransportPort()
                << endl;

                players[adu->getSource().getID()]->addData((char*) adu->getData(), adu->getSize());
                delete adu;
            }
            Thread::sleep(7);
        }
    }

    // redefined from IncomingDataQueue
    void onNewSyncSource(const SyncSource& src)
    {
        cout << "* New synchronization source: " <<
        hex << (int)src.getID() << endl;
        players[src.getID()] = make_unique<StreamPlayer>();
    }

    // redefined from QueueRTCPManager
    void onGotSR(SyncSource& source, SendReport& SR, uint8 blocks)
    {
        RTPSession::onGotSR(source,SR,blocks);
        cout << "I got an SR RTCP report from "
        << hex << (int)source.getID() << "@"
        << dec
        << source.getNetworkAddress() << ":"
        << source.getControlTransportPort() << endl;
    }

    // redefined from QueueRTCPManager
    void onGotRR(SyncSource& source, RecvReport& RR, uint8 blocks)
    {
        RTPSession::onGotRR(source,RR,blocks);
        cout << "I got an RR RTCP report from "
        << hex << (int)source.getID() << "@"
        << dec
        << source.getNetworkAddress() << ":"
        << source.getControlTransportPort() << endl;
    }

    // redefined from QueueRTCPManager
    bool onGotSDESChunk(SyncSource& source, SDESChunk& chunk, size_t len)
    {
        bool result = RTPSession::onGotSDESChunk(source,chunk,len);
        cout << "I got a SDES chunk from "
        << hex << (int)source.getID() << "@"
        << dec
        << source.getNetworkAddress() << ":"
        << source.getControlTransportPort()
        << " ("
        << source.getParticipant()->getSDESItem(SDESItemTypeCNAME)
        << ") " << endl;
        return result;
    }

    void onGotGoodbye(const SyncSource& source, const std::string& reason)
    {
        cout << "I got a Goodbye packet from "
        << hex << (int)source.getID() << "@"
        << dec
        << source.getNetworkAddress() << ":"
        << source.getControlTransportPort() << endl;
        cout << "   Goodbye reason: \"" << reason << "\"" << endl;

        thread([](unique_ptr<StreamPlayer> player) {
            player->finalize();
            while ( !player->dead() ) {
                this_thread::sleep_for(std::chrono::seconds(10));
            }
        }, move(players[source.getID()])).detach();
        players.erase(source.getID());
    }
private:
    unordered_map<int, unique_ptr<StreamPlayer>> players;
};

int main(int argc, char *argv[])
{
    /* start portaudio */
    Pa_Initialize();

    cout << "rtplisten" << endl;

    if (argc != 4) {
        cerr << "Syntax: " << " ip port fname" << endl;
        exit(1);
    }

    InetMcastAddress ima;
    try {
        ima = InetMcastAddress(argv[1]);
    } catch (...) { }

    Listener *foo;
    tpport_t port = atoi(argv[2]);
    if ( ima.isInetAddress() ) {
        foo = new Listener(ima,port);
        cout << "Listening on multicast address " << ima << ":" <<
        port << endl;
    } else {
        InetHostAddress ia(argv[1]);
        foo = new Listener(ia,atoi(argv[2]));
        cout << "Listening on unicast address " << ia << ":" <<
        port << endl;
    }
    cout << "Press Ctrl-C to finish." << endl;
    foo->listen();
    delete foo;
    return 0;
}