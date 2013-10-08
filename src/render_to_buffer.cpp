// This required to get correct linking with Objective-C files
extern "C" {
#include "render_to_buffer.h"
};
// We're probably didn't build libigl.a with LLVM so just use the headers only
// version.
#define IGL_HEADER_ONLY
#include <igl/per_face_normals.h>
#include <igl/normalize_row_lengths.h>
#include <igl/get_seconds.h>
#include <igl/draw_mesh.h>
#include <igl/draw_floor.h>
#include <igl/material_colors.h>
#include <igl/pathinfo.h>
#include <igl/readOBJ.h>
#include <igl/readWRL.h>
#include <igl/triangulate.h>
#include <igl/readOFF.h>
#include <igl/readMESH.h>
#include <igl/boundary_faces.h>
#include <igl/barycenter.h>
#include <igl/doublearea.h>
#include <igl/EPS.h>
#include <igl/camera.h>
#include <igl/canonical_quaternions.h>
#include <igl/quat_to_mat.h>

#include <Eigen/Core>
#include <GL/glu.h>

#include <algorithm>

static int width,height;
static Eigen::MatrixXd V,N;
static Eigen::MatrixXi F;
static Eigen::Vector3d Vmean, Vmax,Vmin;
static bool invert = false;
static float background_color[4] = {0,0,0,1};

// Small viewports struct for keeping track of size and camera info
#define NUM_VIEWPORTS 6
struct Viewport
{
  int x,y,width,height;
  igl::Camera camera;
  Viewport():
    x(0),y(0),width(0),height(0),camera(){};
  Viewport(
    const int x, 
    const int y, 
    const int width,
    const int height, 
    const igl::Camera & camera):
    x(x),
    y(y),
    width(width),
    height(height),
    camera(camera)
  {
  };
  void reshape(
    const int x, 
    const int y, 
    const int width,
    const int height)
  {
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
  };
} viewports[NUM_VIEWPORTS];

// Red screen for errors
void red(const int width, const int height, GLubyte * buffer)
{
  for(int h = 0;h<height;h++)
  {
    for(int w = 0;w<width;w++)
    {
      for(int c = 0;c<4;c++)
      {
        if(c == 0 || c==3)
        {
          buffer[c+4*w+4*width*h] = 255;
        }else
        {
          buffer[c+4*w+4*width*h] = 0;
        }
      }
    }
  }
}

// Initialize the viewport angles and camera rotations
void init_viewports()
{
  using namespace igl;
  using namespace std;
  viewports[0].camera.angle = 10;
  viewports[1].camera.angle = 10;
  viewports[2].camera.angle = 10;
  viewports[3].camera.angle = 10;
  viewports[4].camera.angle = 10;
  viewports[5].camera.angle = 10;
  // Above view
  double XZ_PLANE_QUAT_D_FLIP[4];
  copy(XZ_PLANE_QUAT_D,XZ_PLANE_QUAT_D+4,XZ_PLANE_QUAT_D_FLIP);
  XZ_PLANE_QUAT_D_FLIP[0] *= -1.0;
  // Straight on
  copy(XY_PLANE_QUAT_D,XY_PLANE_QUAT_D+4,viewports[0].camera.rotation);
  // Left side view
  copy(
    CANONICAL_VIEW_QUAT_D[9],
    CANONICAL_VIEW_QUAT_D[9]+4,
    viewports[1].camera.rotation);
  copy(
    CANONICAL_VIEW_QUAT_D[14],
    CANONICAL_VIEW_QUAT_D[14]+4,
    viewports[2].camera.rotation);
  // Straight on
  copy(
    CANONICAL_VIEW_QUAT_D[4],
    CANONICAL_VIEW_QUAT_D[4]+4,
    viewports[3].camera.rotation);
  copy(XZ_PLANE_QUAT_D,XZ_PLANE_QUAT_D+4,viewports[4].camera.rotation);
  copy(XZ_PLANE_QUAT_D_FLIP,XZ_PLANE_QUAT_D_FLIP+4,viewports[5].camera.rotation);
}

// Viewports are arranged to see all sides
//
// /-----.-----.-----\
// |  3  |  2  |  1  |
// -------------------
// |  4  |           |
// -------     0     |
// |  5  |           |
// \-----.-----------/
void reshape_viewports()
{
  using namespace igl;
  using namespace std;
  viewports[0].reshape(1./3.*width,            0,2./3.*width,2./3.*height);
  viewports[1].reshape(2./3.*width, 2./3.*height,1./3.*width,1./3.*height);
  viewports[2].reshape(1./3.*width, 2./3.*height,1./3.*width,1./3.*height);
  viewports[3].reshape(          0, 2./3.*height,1./3.*width,1./3.*height);
  viewports[4].reshape(          0, 1./3.*height,1./3.*width,1./3.*height);
  viewports[5].reshape(          0,            0,1./3.*width,1./3.*height);
}

void reshape(int width, int height)
{
  using namespace std;
  ::width = width;
  ::height = height;
  reshape_viewports();
}

// Simple two-sided, diffuse-only light
void lights()
{
  using namespace std;
  glEnable(GL_LIGHTING);
  glLightModelf(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
  float WHITE[4] =  {0.8,0.8,0.8,1.};
  float GREY[4] =  {0.4,0.4,0.4,1.};
  float BLACK[4] =  {0.,0.,0.,1.};
  float pos[4];
  float light_pos[4] = {0.1,0.1,1.0,0.0};
  copy(light_pos,light_pos+4,pos);
  glLightfv(GL_LIGHT0,GL_AMBIENT,GREY);
  glLightfv(GL_LIGHT0,GL_DIFFUSE,WHITE);
  glLightfv(GL_LIGHT0,GL_SPECULAR,BLACK);
  glLightfv(GL_LIGHT0,GL_POSITION,pos);
  pos[0] *= -1;
  pos[1] *= -1;
  pos[2] *= -1;
  glLightfv(GL_LIGHT1,GL_AMBIENT,GREY);
  glLightfv(GL_LIGHT1,GL_DIFFUSE,WHITE);
  glLightfv(GL_LIGHT1,GL_SPECULAR,BLACK);
  glLightfv(GL_LIGHT1,GL_POSITION,pos);
}

// Push scene based on viewport
void push_scene(const Viewport & vp)
{
  using namespace igl;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  const double angle = vp.camera.angle;
  const double * rot = vp.camera.rotation;
  gluPerspective(angle,(double)width/(double)height,1e-2,10);
  const double z_fix = 2.*tan(angle/2./360.*2.*M_PI);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  // -1 because buffer y-coordinates are flipped
  gluLookAt(0,0,2.3,0,0,0,0,-1,0);
  glPushMatrix();
  double mat[4*4];
  quat_to_mat(rot,mat);
  glMultMatrixd(mat);
  glScaled(z_fix,z_fix,z_fix);
}

// Scale and shift for object so that it fits current view
void push_object(const Viewport & vp)
{
  using namespace Eigen;
  glPushMatrix();
  const double * rot = vp.camera.rotation;
  Quaterniond q(rot[3],rot[0],rot[1],rot[2]);
  Matrix3d m = q.matrix();
  Vector3d eff_Vmax  = m*Vmax;
  Vector3d eff_Vmin  = m*Vmin;
  Vector3d eff_Vmean = m*Vmean;
  const double dy = fabs(eff_Vmax(1,0)-eff_Vmin(1,0));
  const double dx = fabs(eff_Vmax(0,0)-eff_Vmin(0,0));
  //const double dz = fabs(eff_Vmax(2,0)-eff_Vmin(2,0));
  // Assumes height < width
  const double sx = dx*(double)height/(double)width;
  const double sy = dy;
  const double s = 2./(sy > sx ? sy : sx);
  glScaled(s,s,s);
  glTranslated(-Vmean(0,0),-Vmean(1,0),-Vmean(2,0));
  // Hack. Should really just figure out max scale so that full model fits on
  // screen with given perspective.
  //const double dz_off = (dz > 2.*dy && dz > 2.*dx ? dz/2. : 0);
  //glTranslated(0,0,-dz_off);
}

void pop_object()
{
  glPopMatrix();
}

void pop_scene()
{
  glPopMatrix();
}

void display()
{
  using namespace std;
  using namespace igl;
  glClearColor(
    background_color[0],
    background_color[1],
    background_color[2],
    background_color[3]);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  // "Flash light" attached to camera
  lights();
  // Draw for each viewport
  for(int vp = 0;vp<NUM_VIEWPORTS;vp++)
  {
    glViewport(
      viewports[vp].x,
      viewports[vp].y,
      viewports[vp].width,
      viewports[vp].height);
    push_scene(viewports[vp]);
    push_object(viewports[vp]);

    // Set material properties
    glDisable(GL_COLOR_MATERIAL);
    glMaterialfv(GL_FRONT, GL_AMBIENT,  GOLD_AMBIENT);
    glMaterialfv(GL_FRONT, GL_DIFFUSE,  GOLD_DIFFUSE  );
    glMaterialfv(GL_FRONT, GL_SPECULAR, GOLD_SPECULAR);
    glMaterialf (GL_FRONT, GL_SHININESS, 128);
    glMaterialfv(GL_BACK, GL_AMBIENT,  SILVER_AMBIENT);
    glMaterialfv(GL_BACK, GL_DIFFUSE,  FAST_GREEN_DIFFUSE  );
    glMaterialfv(GL_BACK, GL_SPECULAR, SILVER_SPECULAR);
    glMaterialf (GL_BACK, GL_SHININESS, 128);

    // Draw the mesh, inverted if need be.
    if(invert)
    {
      glFrontFace(GL_CW);
    }
    draw_mesh(V,F,N);
    if(invert)
    {
      glFrontFace(GL_CCW);
    }
    pop_object();

    // Draw a nice floor unless we're looking from beneath it
    if(vp != 4)
    {
      glPushMatrix();
      glTranslated(0,-1,0);
      glScaled(4,4,4);
      draw_floor();
      glPopMatrix();
    }

    pop_scene();
  }

  // Screen space
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glViewport(0,0,width,height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0,width,0,height);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Draw separation lines between (around) viewports
  const double BAR_THICKNESS = 3.0;
  glColor3f(0.5,0.5,0.5);
  for(int vp = 0;vp<NUM_VIEWPORTS;vp++)
  {
    glLineWidth(BAR_THICKNESS);
    glBegin(GL_LINE_STRIP);
    glVertex2f(viewports[vp].x,viewports[vp].y);
    glVertex2f(viewports[vp].x+viewports[vp].width,viewports[vp].y);
    glVertex2f(viewports[vp].x+viewports[vp].width,viewports[vp].y+viewports[vp].height);
    glVertex2f(                    viewports[vp].x,viewports[vp].y+viewports[vp].height);
    glEnd();
  }

  glFinish();
}


bool render_to_buffer(
  const char * filename,
  const float * background_color,
  const int width,
  const int height,
  GLubyte * buffer)
{
  using namespace igl;
  using namespace std;
  using namespace Eigen;
  double ts = get_seconds();

  copy(background_color,background_color+4,::background_color);

  // Read and prepare mesh
  // dirname, basename, extension and filename
  string d,b,ext,f;
  pathinfo(filename,d,b,ext,f);
  // Convert extension to lower case
  transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  vector<vector<double > > vV,vN,vTC;
  vector<vector<int > > vF,vFTC,vFN;
  if(ext == "obj")
  {
    // Convert extension to lower case
    if(!igl::readOBJ(filename,vV,vTC,vN,vF,vFTC,vFN))
    {
      red(width,height,buffer);
      return false;
    }
  }else if(ext == "off")
  {
    // Convert extension to lower case
    if(!igl::readOFF(filename,vV,vF,vN))
    {
      red(width,height,buffer);
      return false;
    }
  }else if(ext == "wrl")
  {
    // Convert extension to lower case
    if(!igl::readWRL(filename,vV,vF))
    {
      red(width,height,buffer);
      return false;
    }
  }else
  {
    // Convert extension to lower case
    MatrixXi T;
    if(!igl::readMESH(filename,V,T,F))
    {
      red(width,height,buffer);
      return false;
    }
    //if(F.size() > T.size() || F.size() == 0)
    {
      boundary_faces(T,F);
    }
  }
  if(vV.size() > 0)
  {
    if(!list_to_matrix(vV,V))
    {
      red(width,height,buffer);
      return false;
    }
    triangulate(vF,F);
  }
  cout<<"IO: "<<(get_seconds()-ts)<<"s"<<endl;
  ts = get_seconds();

  // Computer already normalized per triangle normals
  per_face_normals(V,F,N);
  //Vmean = 0.5*(V.colwise().maxCoeff()+V.colwise().minCoeff());
  Vmax = V.colwise().maxCoeff();
  Vmin = V.colwise().minCoeff();
  Vmean = 0.5*(Vmax + Vmin);

  // Figure out if normals should be flipped (hopefully this is never a
  // bottleneck)
  MatrixXd BC;
  VectorXd dblA;
  barycenter(V,F,BC);
  BC.col(0).array() -= Vmean(0,0);
  BC.col(1).array() -= Vmean(1,0);
  BC.col(2).array() -= Vmean(2,0);
  doublearea(V,F,dblA);
  VectorXd BCDN = (BC.array() * N.array()).rowwise().sum();
  const double tot_dp = dblA.transpose() * BCDN;
  invert = tot_dp < 0;
  cout<<"Normals: "<<(get_seconds()-ts)<<"s"<<endl;
  ts = get_seconds();

  // Initialize MESA
  OSMesaContext ctx;
  /* Create an RGBA-mode context */
#  if OSMESA_MAJOR_VERSION * 100 + OSMESA_MINOR_VERSION >= 305
  /* specify Z, stencil, accum sizes */
  ctx = OSMesaCreateContextExt( OSMESA_RGBA, 32, 0, 0, NULL );
#  else
  ctx = OSMesaCreateContext( OSMESA_RGBA, NULL );
#  endif
  if (!ctx)
  {
    fprintf(stderr,"OSMesaCreateContext failed!\n");
    red(width,height,buffer);
    return false;
  }

  /* Bind the buffer to the context and make it current */
  if (!OSMesaMakeCurrent( ctx, buffer, GL_UNSIGNED_BYTE, width, height))
  {
    fprintf(stderr,"OSMesaMakeCurrent failed!\n");
    red(width,height,buffer);
    return false;
  }
    
  // Render
  init_viewports();
  reshape(width,height);
  display();
  cout<<"Display: "<<(get_seconds()-ts)<<"s"<<endl;
  ts = get_seconds();

  /* destroy the context */
  OSMesaDestroyContext( ctx );
  
  return true;
}
