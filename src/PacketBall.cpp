#include "cinder/app/AppScreenSaver.h"
#include "cinder/app/AppBasic.h"
#include "cinder/Camera.h"
#include "cinder/Rand.h"
#include "cinder/Vector.h"
#include "ShapeLib/shapefil.h"
#include "pcap/pcap.h"
#include "geoip.h"

using namespace std;
using namespace cinder;
using namespace cinder::app;

#ifdef SCREENSAVER
    #define APP_TYPE    AppScreenSaver
#else
    #define APP_TYPE    AppBasic
#endif

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

class PacketBall : public APP_TYPE
{
public:
    virtual void setup();
    virtual void shutdown();
    virtual void update();
    virtual void draw();
    
private:
    Timer timer_;
    float rotation_;
    SHPHandle shapefile_;
    int entityCount_, shapeType_;
    double minBounds_[4], maxBounds_[4];
    pcap_t *capture_;
    FILE *fp_;
    GeoIp geoIp_;
};

void PacketBall::setup()
{
    Rand::randomize();

    shapefile_ = SHPOpen("GSHHS_c_L1", "rb");
    SHPGetInfo(shapefile_, &entityCount_, &shapeType_, minBounds_, maxBounds_);

    fp_ = fopen("test.txt", "w");

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *devices;
    if (pcap_findalldevs(&devices, errbuf) == -1)
        exit(1);
    int i;
    pcap_if_t *d;
    for (i = 0, d = devices; d != NULL; d = d->next)
        fprintf(fp_, "%s\n", d->name);
    capture_ = pcap_open_live(devices->name, 100, 0, 20, errbuf);
    fprintf(fp_, "DEV: %s %d\n", devices->name, capture_);
    fflush(fp_);
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

void PacketBall::shutdown()
{
    fclose(fp_);
}

void PacketBall::update()
{
    timer_.stop();
    float msecs = 1000.0f * static_cast<float>(timer_.getSeconds());
    timer_.start();

    rotation_ += (msecs / 100);
    if (rotation_ > 360)
        rotation_ -= 360;

    pcap_pkthdr *header = NULL;
    const unsigned char *data = NULL;
    while (pcap_next_ex(capture_, &header, &data) > 0)
    {
        ip_header *ip = (ip_header *)(data + 14);

        Location src = geoIp_.Lookup(ip->saddr.byte);
        Location dst = geoIp_.Lookup(ip->daddr.byte);

        fprintf(fp_, "Data: %d.%d.%d.%d (%s - %s %f/%f)-> %d.%d.%d.%d (%s %s %f/%f)\n", 
            ip->saddr.byte[0], ip->saddr.byte[1], ip->saddr.byte[2], ip->saddr.byte[3], src.countryName.c_str(), src.city.c_str(), src.latitude, src.longitude, 
            ip->daddr.byte[0], ip->daddr.byte[1], ip->daddr.byte[2], ip->daddr.byte[3], dst.countryName.c_str(), dst.city.c_str(), dst.latitude, dst.longitude);
        fflush(fp_);
    }
}

void PacketBall::draw()
{
    gl::clear();

    CameraPersp cam = CameraPersp(getWindowWidth(), getWindowHeight(), 90, 0.5, 3);
    cam.lookAt(Vec3f(1.75, 0, 0), Vec3f(0, 0, 0), Vec3f(0, 1, 0));
    gl::setMatrices(cam);
    gl::color(1, 1, 1);
    gl::rotate(Vec3f(0, rotation_, 0));

    for (int i = 0; i < entityCount_; i ++)
    {
        SHPObject *obj = SHPReadObject(shapefile_, i);
        glBegin(GL_LINE_STRIP);
        for (int j = 0; j < obj->nVertices; j ++)
        {
            float x = obj->padfX[j] * M_PI / 180;
            float y = obj->padfY[j] * M_PI / 180;
            glVertex3f(math<float>::sin(x) * math<float>::cos(y), math<float>::sin(y), math<float>::cos(x) * math<float>::cos(y));
        }
        glEnd();
        SHPDestroyObject(obj);
    }
}

#ifdef SCREENSAVER
    CINDER_APP_SCREENSAVER(PacketBall, RendererGl)
#else
    CINDER_APP_BASIC(PacketBall, RendererGl)
#endif
