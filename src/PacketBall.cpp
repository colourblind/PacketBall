#include "cinder/app/AppScreenSaver.h"
#include "cinder/app/AppBasic.h"
#include "cinder/Camera.h"
#include "cinder/Rand.h"
#include "cinder/Vector.h"
#include "ShapeLib/shapefil.h"

using namespace std;
using namespace cinder;
using namespace cinder::app;

#ifdef SCREENSAVER
    #define APP_TYPE    AppScreenSaver
#else
    #define APP_TYPE    AppBasic
#endif

class PacketBall : public APP_TYPE
{
public:
    virtual void setup();
    virtual void update();
    virtual void draw();
    
private:
    Timer timer_;
    float rotation_;
    SHPHandle shapefile_;
    int entityCount_, shapeType_;
    double minBounds_[4], maxBounds_[4];
};

void PacketBall::setup()
{
    Rand::randomize();

    shapefile_ = SHPOpen("GSHHS_c_L1", "rb");
    SHPGetInfo(shapefile_, &entityCount_, &shapeType_, minBounds_, maxBounds_);

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

    rotation_ += (msecs / 100);
    if (rotation_ > 360)
        rotation_ -= 360;
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
