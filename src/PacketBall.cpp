#include "Window.h"
#include "World.h"
#include "Utils.h"
#include "ShapeLib/shapefil.h"
#include "pcap/pcap.h"
#include "geoip.h"
#include <vector>
#include <sstream>
#include <memory>

using namespace OnceMoreWithFeeling;
using namespace std;

const float PING_LIFETIME = 10000;
const int SPLINE_DETAIL = 25;

struct ip_address{
    unsigned char byte[4];
};

struct ip_header{
    unsigned char   ver_ihl;        // Version (4 bits) + Internet header length (4 bits)
    unsigned char   tos;            // Type of service 
    unsigned short  tlen;           // Total length 
    unsigned short  identification; // Identification
    unsigned short  flags_fo;       // Flags (3 bits) + Fragment offset (13 bits)
    unsigned char   ttl;            // Time to live
    unsigned char   proto;          // Protocol
    unsigned short  crc;            // Header checksum
    ip_address      saddr;          // Source address
    ip_address      daddr;          // Destination address
    unsigned int    op_pad;         // Option + Padding
};

struct Ping
{
    Ping(Vector home, Vector away, string t) : lifetime(PING_LIFETIME), text(t)
    {
        Vector middle = home + ((away - home) * 0.5f);
        middle = middle.Normalise() * 1.2f;
            
        vector<Vector> points;
        points.push_back(home);
        points.push_back(middle);
        points.push_back(away);
        Spline s(points);
        vector<Vector> splinePoints = s.GetPoints(SPLINE_DETAIL);
        vector<float> sigh;
        for (auto v : splinePoints)
        {
            sigh.push_back(v.x);
            sigh.push_back(v.y);
            sigh.push_back(v.z);
        }
        
        shared_ptr<Buffer> b = make_shared<Buffer>();
        b->SetData(sigh);
        shared_ptr<Object> o = make_shared<Object>();
        o->AttachBuffer(0, b);
        spline = make_shared<RenderObject>();
        spline->object = o;
        spline->program = "spline|spline";
        spline->colour[0] = 0;
        spline->colour[1] = 1;
        spline->colour[2] = 0;
    }

    float lifetime;
    string text;
    shared_ptr<RenderObject> spline;
};

float planeVerts[] = {
    -0.5f, -0.5f, 0,
    -0.5f, 0.5f, 0,
    0.5f, 0.5f, 0,
    0.5f, 0.5f, 0,
    0.5f, -0.5f, 0,
    -0.5f, -0.5f, 0
};

float planeTexCoords[] = {
    0, 0,
    0, 1,
    1, 1,
    1, 1,
    1, 0,
    0, 0
};

class PacketBall : public World
{
public:
    virtual void Init(shared_ptr<Renderer> renderer);
    virtual void Upate(float msecs);
    virtual void Draw(shared_ptr<Renderer> renderer);
    
private:
    Vector ConvertLatLong(double lat, double lon);
    string GetPacketString(ip_header *packet, Location src, Location dst);

    vector<shared_ptr<RenderObject>> landmasses_;
    shared_ptr<RenderObject> renderPing_;
    vector<Ping> pings_;
    float rotation_;
    pcap_t *capture_;
    GeoIp geoIp_;
};

void PacketBall::Init(shared_ptr<Renderer> renderer)
{
    glEnable(GL_DEPTH_TEST);

    renderer->SetCameraPosition(Vector(0, 0, 2.5f));
    renderer->AddTexture("ping.png");
    renderer->AddShader("basic", "basic");
    renderer->AddShader("ping", "ping");
    renderer->AddShader("spline", "spline");

    shared_ptr<Buffer> pingVerts = make_shared<Buffer>();
    shared_ptr<Buffer> pingTexCoords = make_shared<Buffer>();
    pingVerts->SetData(planeVerts, 18);
    pingTexCoords->SetData(planeTexCoords, 12);
    shared_ptr<Object> pingObject = make_shared<Object>();
    pingObject->AttachBuffer(0, pingVerts);
    pingObject->AttachBuffer(1, pingTexCoords, 2);
    renderPing_ = make_shared<RenderObject>();
    renderPing_->object = pingObject;
    renderPing_->program = "ping|ping";
    renderPing_->colour[0] = 1;
    renderPing_->colour[1] = 1;
    renderPing_->colour[2] = 0.5f;

    int entityCount, shapeType;
    double minBounds[4], maxBounds[4];
    SHPHandle shapefile = SHPOpen("GSHHS_c_L1", "rb");
    SHPGetInfo(shapefile, &entityCount, &shapeType, minBounds, maxBounds);

    for (int i = 0; i < entityCount; i++)
    {
        vector<float> points;

        SHPObject *obj = SHPReadObject(shapefile, i);
        for (int j = 0; j < obj->nVertices; j++)
        {
            Vector v = ConvertLatLong(obj->padfY[j], obj->padfX[j]);
            points.push_back(v.x);
            points.push_back(v.y);
            points.push_back(v.z);
        }
        SHPDestroyObject(obj);

        shared_ptr<Buffer> buffer = make_shared<Buffer>();
        buffer->SetData(points);
        shared_ptr<Object> object = make_shared<Object>();
        object->AttachBuffer(0, buffer);

        shared_ptr<RenderObject> renderObject = make_shared<RenderObject>();
        renderObject->object = object;
        renderObject->program = "basic|basic";
        renderObject->colour[0] = renderObject->colour[1] = renderObject->colour[2] = 1;

        landmasses_.push_back(renderObject);
    }

    SHPClose(shapefile);

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *devices;
    if (pcap_findalldevs(&devices, errbuf) == -1)
        exit(1);
    capture_ = pcap_open_live(devices->name, 100, 0, 5, errbuf);
    pcap_freealldevs(devices);

    geoIp_.Init();

    rotation_ = 0;
}

void PacketBall::Upate(float msecs)
{
    rotation_ += msecs * 0.0004f;
    if (rotation_ > PI * 2)
        rotation_ -= PI * 2;

    pcap_pkthdr *header = NULL;
    const unsigned char *data = NULL;
    while (pcap_next_ex(capture_, &header, &data) > 0)
    {
        ip_header *ip = (ip_header *)(data + 14);

        // Discard packets that aren't IPv4
        if ((ip->ver_ihl >> 4) != 4)
            continue;

        Location src = geoIp_.Lookup(ip->saddr.byte);
        Location dst = geoIp_.Lookup(ip->daddr.byte);
        string s = GetPacketString(ip, src, dst);

        // Look for an existing ping
        Ping *p = NULL;
        for (vector<Ping>::iterator iter = pings_.begin(); iter != pings_.end(); iter ++)
        {
            if (iter->text == s)
            {
                p = &(*iter);
                break;
            }
        }

        if (p == NULL)
        {
            // YOU LIVE IN GREENWICH NOW. NO ARGUMENTS.
            Vector home(ConvertLatLong(51.48f, 0));
            Vector away;
            if (src.latitude >= -180)
                away = ConvertLatLong(src.latitude, src.longitude);
            if (dst.latitude >= -180)
                away = ConvertLatLong(dst.latitude, dst.longitude);
            
            pings_.push_back(Ping(home, away, s));
        }
        else
        {
            p->lifetime = PING_LIFETIME;
        }
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

void PacketBall::Draw(shared_ptr<Renderer> renderer)
{
    for (auto r : landmasses_)
    {
        r->transformation = Matrix::Rotate(0, rotation_, 0);
        renderer->Draw(r, GL_LINE_STRIP);
    }

    /*
    unordered_map<unsigned int, string> bindings;
    bindings.insert(make_pair(0, "ping.png"));
    renderer->SetTextures("ping|ping", bindings);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    for (auto p : pings_)
    {   
        Vector modelSpacePosition = Matrix::Rotate(0, rotation_, 0) * p.position;
        Matrix billboard = Matrix::Billboard(Vector(0, 0, 2.5f), modelSpacePosition);
        renderPing_->transformation = Matrix::Translate(modelSpacePosition) * billboard * Matrix::Scale(0.2f);
        renderer->SetUniform("ping|ping", 1, p.lifetime / PING_LIFETIME);
        renderer->Draw(renderPing_);
    }
    glDepthMask(GL_TRUE);
    */
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (auto p : pings_)
    {
        p.spline->transformation = Matrix::Rotate(0, rotation_, 0);
        renderer->SetUniform(p.spline->program, 0, p.lifetime / PING_LIFETIME);
        renderer->Draw(p.spline, GL_LINE_STRIP);
    }
    /*
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

    gl::disableDepthWrite();

    Vec3f right, up;
    cam.getBillboardVectors(&right, &up);
    pingTexture_.enableAndBind();
    for (vector<Ping>::iterator iter = pings_.begin(); iter != pings_.end(); iter ++)
    {
        gl::color(1, 1, 0.5f, iter->lifetime / PING_LIFETIME);
        gl::drawBillboard(iter->position, Vec2f(0.2f, 0.2f), 0, right, up);
    }
    pingTexture_.disable();

    gl::setMatricesWindow(getWindowWidth(), getWindowHeight());
    float y_offset = 0;
    Font f("Arial", 16);
    for (vector<Ping>::iterator iter = pings_.begin(); iter != pings_.end(); iter ++)
    {
        gl::drawString(iter->text, Vec2f(10, 10 + y_offset), ColorA(1, 1, 1, 1), f);
        y_offset += 20;
    }

    gl::enableDepthWrite();
    */
}

Vector PacketBall::ConvertLatLong(double lat_rads, double lon_rads)
{
    float lon = static_cast<float>(lon_rads) * PI / 180;
    float lat = static_cast<float>(lat_rads) * PI / 180;
    return Vector(sin(lon) * cos(lat), sin(lat), cos(lon) * cos(lat));
}

string PacketBall::GetPacketString(ip_header *packet, Location src, Location dst)
{
    stringstream ss;
    ss << static_cast<short>(packet->saddr.byte[0]) << "." << static_cast<short>(packet->saddr.byte[1]) << "." << static_cast<short>(packet->saddr.byte[2]) << "." << static_cast<short>(packet->saddr.byte[3]);
    ss << " -> ";
    ss << static_cast<short>(packet->daddr.byte[0]) << "." << static_cast<short>(packet->daddr.byte[1]) << "." << static_cast<short>(packet->daddr.byte[2]) << "." << static_cast<short>(packet->daddr.byte[3]);
    ss << " (" << src.countryName << ") (" << dst.countryName << ")";
    return ss.str();
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR commandLine, int show)
{
    srand(::GetTickCount());

    OnceMoreWithFeeling::Window w(instance, show);
    shared_ptr<Renderer> renderer = make_shared<Renderer>();
    shared_ptr<World> world = make_shared<PacketBall>();

    world->Init(renderer);
    return w.Loop(world, renderer);
}
