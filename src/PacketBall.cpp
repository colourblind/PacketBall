#include "cinder/app/AppScreenSaver.h"
#include "cinder/app/AppBasic.h"
#include "cinder/Camera.h"
#include "cinder/Rand.h"
#include "cinder/Vector.h"
#include "ShapeLib/shapefil.h"
#include "pcap/pcap.h"
#include "geoip.h"
#include <vector>

using namespace std;
using namespace cinder;
using namespace cinder::app;

#ifdef SCREENSAVER
    #define APP_TYPE    AppScreenSaver
#else
    #define APP_TYPE    AppBasic
#endif

const float PING_LIFETIME = 3000;

struct ip_address{
    unsigned char byte[4];
};

struct ip_header{
    unsigned char  ver_ihl;        // Version (4 bits) + Internet header length (4 bits)
    unsigned char  tos;            // Type of service 
    unsigned short tlen;           // Total length 
    unsigned short identification; // Identification
    unsigned short flags_fo;       // Flags (3 bits) + Fragment offset (13 bits)
    unsigned char  ttl;            // Time to live
    unsigned char  proto;          // Protocol
    unsigned short crc;            // Header checksum
    ip_address  saddr;      // Source address
    ip_address  daddr;      // Destination address
    unsigned int   op_pad;         // Option + Padding
};

struct Ping
{
    Ping(Vec3f p) : position(p), lifetime(PING_LIFETIME) { }

    Vec3f position;
    float lifetime;
};

class PacketBall : public APP_TYPE
{
public:
    virtual void setup();
    virtual void update();
    virtual void draw();
    
private:
    Vec3f ConvertLatLong(double lat, double lon);

    vector<Ping> pings_;
    Timer timer_;
    float rotation_;
    SHPHandle shapefile_;
    int entityCount_, shapeType_;
    double minBounds_[4], maxBounds_[4];
    pcap_t *capture_;
    GeoIp geoIp_;
};

void PacketBall::setup()
{
    Rand::randomize();

    shapefile_ = SHPOpen("GSHHS_c_L1", "rb");
    SHPGetInfo(shapefile_, &entityCount_, &shapeType_, minBounds_, maxBounds_);

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *devices;
    if (pcap_findalldevs(&devices, errbuf) == -1)
        exit(1);
    capture_ = pcap_open_live(devices->name, 100, 0, 20, errbuf);
    pcap_freealldevs(devices);

    geoIp_.Init();

    rotation_ = 0;

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, 0.5);
    glHint(GL_FOG_HINT, GL_NICEST); 

    gl::enableDepthRead();
    gl::enableDepthWrite();

    gl::enableAlphaBlending();
}

void PacketBall::update()
{
    timer_.stop();
    float msecs = 1000.0f * static_cast<float>(timer_.getSeconds());
    timer_.start();

    rotation_ += (msecs / 10000);
    if (rotation_ > M_PI * 2)
        rotation_ -= M_PI * 2;

    pcap_pkthdr *header = NULL;
    const unsigned char *data = NULL;
    while (pcap_next_ex(capture_, &header, &data) > 0)
    {
        ip_header *ip = (ip_header *)(data + 14);

        Location src = geoIp_.Lookup(ip->saddr.byte);
        Location dst = geoIp_.Lookup(ip->daddr.byte);

        if (src.latitude >= -180)
            pings_.push_back(Ping(ConvertLatLong(src.latitude, src.longitude)));
        if (dst.latitude >= -180)
            pings_.push_back(Ping(ConvertLatLong(dst.latitude, dst.longitude)));
    }

    vector<Ping>::iterator iter = pings_.begin();
    while (iter != pings_.end())
    {
        iter->lifetime -= msecs;
        if (iter->lifetime < 0)
            iter = pings_.erase(iter);
        else
            iter ++;
    }
}

void PacketBall::draw()
{
    gl::clear();

    CameraPersp cam = CameraPersp(getWindowWidth(), getWindowHeight(), 90, 0.5, 3);

    Vec3f camPos = Vec3f(math<float>::cos(rotation_), 0, math<float>::sin(rotation_)) * 1.75f;

    cam.lookAt(camPos, Vec3f(0, 0, 0), Vec3f(0, 1, 0));
    gl::setMatrices(cam);
    gl::color(1, 1, 1);

    for (int i = 0; i < entityCount_; i ++)
    {
        SHPObject *obj = SHPReadObject(shapefile_, i);
        glBegin(GL_LINE_STRIP);
        for (int j = 0; j < obj->nVertices; j ++)
        {
            Vec3f pos = ConvertLatLong(obj->padfY[j], obj->padfX[j]);
            glVertex3f(pos.x, pos.y, pos.z);
        }
        glEnd();
        SHPDestroyObject(obj);
    }

    Vec3f right, up;
    cam.getBillboardVectors(&right, &up);
    for (vector<Ping>::iterator iter = pings_.begin(); iter != pings_.end(); iter ++)
    {
        gl::color(1, 1, 0, iter->lifetime / PING_LIFETIME);
        gl::drawBillboard(iter->position, Vec2f(0.1f, 0.1f), 0, right, up);
    }
}

Vec3f PacketBall::ConvertLatLong(double lat_rads, double lon_rads)
{
    float lon = lon_rads * M_PI / 180;
    float lat = lat_rads * M_PI / 180;
    return Vec3f(math<float>::sin(lon) * math<float>::cos(lat), math<float>::sin(lat), math<float>::cos(lon) * math<float>::cos(lat));
}

#ifdef SCREENSAVER
    CINDER_APP_SCREENSAVER(PacketBall, RendererGl)
#else
    CINDER_APP_BASIC(PacketBall, RendererGl)
#endif
