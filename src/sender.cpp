#include <cstdlib>
#include <ccrtp/rtp.h>
#include <atomic>
#include <signal.h>
#include <sndfile.h>


using namespace ost;
using namespace std;

atomic_bool runcond{true};

class Sender: public RTPSession, public TimerPort
{
public:
    Sender(const InetHostAddress& ia, tpport_t port) :
    RTPSession(InetHostAddress("0.0.0.0")), packetsPerSecond(100), timestamp(0), period(1000 / packetsPerSecond)
    {
        cout << "My SSRC identifier is: "
             << hex << (int)getLocalSSRC() << endl;

        defaultApplication().setSDESItem(SDESItemTypeTOOL, "rtpsend demo app.");
        setSchedulingTimeout(10000);
        setExpireTimeout(1000000);

        if ( !addDestination(ia,port) ) {
            cerr << "Could not connect" << endl;
            exit();
        }


        tstampInc = getCurrentRTPClockRate()/packetsPerSecond;
        TimerPort::setTimer(period);

        setPayloadFormat(StaticPayloadFormat(sptPCMU));
        startRunning();
    }

    using RTPSession::putData;
    void putData(const unsigned char* data, size_t len) {
        cerr << "[" << timestamp <<  "] Put data " << len << "\n";
        putData(timestamp, data, len);
        timestamp += tstampInc;
        Thread::sleep(TimerPort::getTimer());
        TimerPort::incTimer(period);
    }

private:
    const uint16 packetsPerSecond;
    uint32 timestamp;
    uint32 period;
    uint16 tstampInc;
};

void stopHandler(int) {
    runcond = false;
}

int main(int argc, char *argv[]) {
    if (argc<2){
        cerr << "Syntax: " << "file host port" << endl;
        return -1;
    }

    SF_INFO sfInfo;
    sfInfo.format = 0;
    auto position = 0;
    auto sndFile = sf_open(argv[1], SFM_READ, &sfInfo);

    if (!sndFile) {
        printf("error opening file\n");
        return 1;
    }

    signal(SIGINT, stopHandler);
    Sender sender(InetHostAddress(argv[2]), atoi(argv[3]));

    constexpr auto bsize = 1024*8;
    int buffer[bsize];
    while ( position < sfInfo.frames && runcond ) {
        auto toRead = bsize / sfInfo.channels;
        if (toRead > (sfInfo.frames - position)) {
            toRead = sfInfo.frames - position;
        }
        if ( auto readed = sf_readf_int(sndFile, buffer, toRead) ) {
            position += readed;
            sender.putData(reinterpret_cast<unsigned char*>(buffer), readed*sizeof(int)*sfInfo.channels);
        } else {
            runcond = false;
        }
    }

    return 0;
}
