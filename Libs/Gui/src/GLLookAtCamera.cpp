/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#include <Visus/GLLookAtCamera.h>
#include <Visus/Log.h>

namespace Visus {

//////////////////////////////////////////////////
void GLLookAtCamera::executeAction(StringTree in)
{
  if (in.name == "setLookAt")
  {
    auto pos    = Point3d::fromString(in.readString("pos"));
    auto center = Point3d::fromString(in.readString("center"));
    auto vup    = Point3d::fromString(in.readString("vup"));
    auto quaternion = Quaternion::fromString(in.readString("quaternion"));
    setLookAt(pos, center, vup, quaternion);
    return;
  }

  if (in.name == "walk")
  {
    auto direction = Point3d::fromString(in.readString("direction"));
    walk(direction);
    return;
  }

  if (in.name == "rotate")
  {
    auto angle = Utils::degreeToRadiant(in.readDouble("angle"));

#if 0
    if (in.hasAttribute("screen_center"))
    {
      auto screen_center = Point2d::fromString(in.readString("screen_center"));
      rotateAroundScreenCenter(angle, screen_center);
      return;
    }
#endif

    auto axis = Point3d::fromString(in.readString("axis"));
    rotate(angle, axis);
    return;
  }

  if (in.name=="guessPosition")
  {
    auto bounds = BoxNd::fromString(in.readString("bounds"));
    auto ref = in.readInt("ref");
    guessPosition(bounds, ref);
    return;
  }

  if (in.name == "set")
  {
    auto target_id = in.read("target_id");

    if (target_id == "viewport")
    {
      auto value = Viewport::fromString(in.read("value"));
      setViewport(value);
      return;
    }

    if (target_id == "use_ortho_projection")
    {
      auto value = in.readBool("value");
      setUseOrthoProjection(value);
      return;
    }

    if (target_id == "ortho_params")
    {
      auto value = GLOrthoParams::fromString(in.read("value"));
      setOrthoParams(value);
      return;
    }

    if (target_id == "ortho_params_fixed")
    {
      auto value = in.readBool("value");
      setOrthoParamsFixed(value);
      return;
    }

    if (target_id == "bounds")
    {
      auto value = BoxNd::fromString(in.readString("value"));
      setBounds(value);
      return;
    }
  }

  return GLCamera::executeAction(in);
}


//////////////////////////////////////////////////////////////////////
std::pair<double, double> GLLookAtCamera::guessNearFarDistance() const
{
  auto dir= (this->center - this->pos).normalized();
  Point3d far_point ((dir[0] >= 0) ? bounds.p2[0] : bounds.p1[0], (dir[1] >= 0) ? bounds.p2[1] : bounds.p1[1], (dir[2] >= 0) ? bounds.p2[2] : bounds.p1[2]);
  Point3d near_point((dir[0] >= 0) ? bounds.p1[0] : bounds.p2[0], (dir[1] >= 0) ? bounds.p1[1] : bounds.p2[1], (dir[2] >= 0) ? bounds.p1[2] : bounds.p2[2]);
  Plane camera_plane(dir, this->pos);
  double near_dist = camera_plane.getDistance(near_point);
  double far_dist = camera_plane.getDistance(far_point);
  return std::make_pair(near_dist, far_dist);
}



//////////////////////////////////////////////////////////////////////
double GLLookAtCamera::guessForwardFactor() const
{
  auto   pair = guessNearFarDistance();

  double near_dist = pair.first;
  double far_dist = pair.second;

  // update the sensitivity of moving forward

  // when camera inside volume
  // roughly means 64 scroll steps to go through the volume
  if (near_dist <= 0)
    return (far_dist - near_dist) / 64;

  // if the camera is far away from the volume, it moves toward the volume faster
  else
    return far_dist / 16;
}


//////////////////////////////////////////////////////////////////////
GLOrthoParams GLLookAtCamera::guessOrthoParams() const
{
  auto old_value = this->ortho_params;

  auto   pair = guessNearFarDistance();
  double near_dist = pair.first;
  double far_dist = pair.second;

  double ratio = getViewport().width / (double)getViewport().height;
  const double fov = 60.0;

  auto ret = GLOrthoParams();
  ret.zNear  = near_dist <= 0 ? 0.1 : near_dist;
  ret.zFar   = far_dist + far_dist - near_dist;
  ret.top    = ret.zNear * tan(fov * Math::Pi / 360.0);
  ret.bottom = -ret.top;
  ret.left   = ret.bottom * ratio;
  ret.right  = ret.top * ratio;

  return ret;
}


//////////////////////////////////////////////////
void GLLookAtCamera::setViewport(Viewport new_value)
{
  auto& old_value=this->viewport;
  if (old_value == new_value)
    return;

  beginUpdate(
    StringTree("set").write("target_id", "viewport").write("value", new_value.toString()),
    StringTree("set").write("target_id", "viewport").write("value", old_value.toString()));
  {
    old_value = new_value;
    if (!ortho_params_fixed)
      setOrthoParams(guessOrthoParams());
  }
  endUpdate();
}



//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::setUseOrthoProjection(bool new_value)
{
  auto& old_value = this->use_ortho_projection;
  if (new_value== old_value)
    return;
 
  beginUpdate(
    StringTree("set").write("target_id", "use_ortho_projection").write("value", cstring(new_value)),
    StringTree("set").write("target_id", "use_ortho_projection").write("value", cstring(old_value)));
  {
    old_value = new_value;
    if (!ortho_params_fixed)
      setOrthoParams(guessOrthoParams());
  }
  endUpdate();
}

//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::setOrthoParams(GLOrthoParams new_value) 
{
  auto& old_value = this->ortho_params;
  if (old_value == new_value)
    return;

  beginUpdate(
    StringTree("set").write("target_id", "ortho_params").write("value", new_value.toString()),
    StringTree("set").write("target_id", "ortho_params").write("value", old_value.toString()));
  {
    old_value = new_value;
  }
  endUpdate();
}

//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::setOrthoParamsFixed(bool new_value) 
{
  auto& old_value = this->ortho_params_fixed;
  if (new_value ==old_value)
    return;
 
  beginUpdate(
    StringTree("set").write("target_id", "ortho_params_fixed").write("value", cstring(new_value)),
    StringTree("set").write("target_id", "ortho_params_fixed").write("value", cstring(old_value)));
  {
    old_value = new_value;
    if (!ortho_params_fixed)
      setOrthoParams(guessOrthoParams());
  }
  endUpdate();
}

//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::glMousePressEvent(QMouseEvent* evt)
{
  this->mouse.glMousePressEvent(evt);
  evt->accept();
  last_mouse_pos[evt->button()] = Point2i(evt->x(),evt->y());
}

//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::glMouseMoveEvent(QMouseEvent* evt)
{
  int button=
    (evt->buttons() & Qt::LeftButton  )? Qt::LeftButton  : 
    (evt->buttons() & Qt::RightButton )? Qt::RightButton : 
    (evt->buttons() & Qt::MiddleButton)? Qt::MiddleButton:
    0;

  if (!button) 
    return ;

  this->mouse.glMouseMoveEvent(evt);

  int W=getViewport().width;
  int H=getViewport().height;


  if (this->mouse.getButton(Qt::LeftButton).isDown && button == Qt::LeftButton) 
  {
    Point2i screen_p1 = last_mouse_pos[button];
    Point2i screen_p2 = this->mouse.getButton(button).pos;
    last_mouse_pos[button] = screen_p2;

    double R = ortho_params.getWidth() / 2.0;

    // project the center of rotation to the screen
    Frustum temp;
    temp.setViewport(getViewport());
    temp.loadProjection(Matrix::perspective(60, W / (double)H, ortho_params.zNear, ortho_params.zFar));
    temp.loadModelview(Matrix::lookAt(this->pos, this->center, this->vup));

    FrustumMap map(temp);
    Point3d center = map.unprojectPointToEye(map.projectPoint(this->center));
    Point3d p1 = map.unprojectPointToEye(screen_p1.castTo<Point2d>());
    Point3d p2 = map.unprojectPointToEye(screen_p2.castTo<Point2d>());

    double x1 = p1[0] - center[0], y1 = p1[1] - center[1]; double l1 = x1 * x1 + y1 * y1;
    double x2 = p2[0] - center[0], y2 = p2[1] - center[1]; double l2 = x2 * x2 + y2 * y2;
    p1[2] = (l1 < R * R / 2) ? sqrt(R * R - l1) : R * R / 2 / sqrt(l1);
    p2[2] = (l2 < R * R / 2) ? sqrt(R * R - l2) : R * R / 2 / sqrt(l2);

    auto M = Matrix::lookAt(this->pos, this->center, this->vup).invert();
    Point3d a = (M * p1 - M * center).normalized();
    Point3d b = (M * p2 - M * center).normalized();

    auto axis = a.cross(b).normalized();
    auto angle = acos(a.dot(b));

    if (!axis.module() || !axis.valid())
    {
      evt->accept();
      return;
    }

    rotate(angle, axis);

    evt->accept();
    return;
  }

  // pan
  if (this->mouse.getButton(Qt::RightButton).isDown && button==Qt::RightButton) 
  {
    Point2i p1 = last_mouse_pos[button];
    Point2i p2 = this->mouse.getButton(button).pos;
    last_mouse_pos[button] = p2;

    Point3d vt;
    vt.x = ((p1.x - p2.x) / getViewport().width ) * guessForwardFactor() * pan_factor;
    vt.y = ((p1.y - p2.y) / getViewport().height) * guessForwardFactor() * pan_factor;
    vt.z = 0.0;

    walk(vt);
    evt->accept();
    return;
  }

  last_mouse_pos[button] = Point2i(evt->x(),evt->y());
}



//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::glMouseReleaseEvent(QMouseEvent* evt)
{
  this->mouse.glMouseReleaseEvent(evt);
  evt->accept();
  last_mouse_pos[evt->button()] = Point2i(evt->x(),evt->y());
}

//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::glWheelEvent(QWheelEvent* evt)
{
  Point3d vt;
  vt.x = 0.0;
  vt.y = 0.0;
  vt.z = (evt->delta() < 0 ? -1 : +1) * guessForwardFactor();
  walk(vt);
  evt->accept();
}

//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::glKeyPressEvent(QKeyEvent* evt) 
{
  int key=evt->key();

  //moving the projection
  if  (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down ) 
  {
    double dx=0,dy=0;
    if (key==Qt::Key_Left ) dx=-ortho_params.getWidth();
    if (key==Qt::Key_Right) dx=+ortho_params.getWidth();
    if (key==Qt::Key_Up   ) dy=+ortho_params.getHeight();
    if (key==Qt::Key_Down ) dy=-ortho_params.getHeight();

    auto ortho_params=this->ortho_params;
    ortho_params.translate(Point3d(dx,dy));
    setOrthoParams(ortho_params);
    evt->accept(); 
    return; 
  }

  switch(key)
  {
    // TODO: Use double-tap to accomplish same thing on iPhone.
    case Qt::Key_X: // recenter the camera
    {
      // TODO: No access to scene, so just set center of rotation
      // at current distance, center of screen. Would like to
      // intersect ray with bounding volumes of scene components and
      // use an actual point on/in one of these volumes, but all we
      // have is the global bound. Sometimes this is acceptable
      // (e.g. a section of neural microscopy), but it's not always
      // sufficient (e.g. large volume/more complicated scene).

      // TODO: really want to use mouse position for center, not just center of screen.
      int W=getViewport().width/2;
      int H=getViewport().height/2;
      auto ray=FrustumMap(getCurrentFrustum()).getRay(Point2d(W,H));

      RayBoxIntersection intersection(ray,bounds);
      if (intersection.valid)
      {
        Point3d pos, center, vup;
        getLookAt(pos,center,vup);
        center = ray.getPoint(0.5 * (std::max(intersection.tmin, 0.0) + intersection.tmax)).toPoint3();
        setLookAt(pos, center, vup);
      }

      evt->accept();
      return ;
    }
    case Qt::Key_P:
    {
      VisusInfo() << this->encode().toString();
      evt->accept();
      return;
    }

    case Qt::Key_M:
    {
      setUseOrthoProjection(!this->use_ortho_projection);
      evt->accept();
      return;
    }
  }
}


//////////////////////////////////////////////////////////////////////
bool GLLookAtCamera::guessPosition(BoxNd bounds,int ref) 
{
  bounds.setPointDim(3);

  Point3d pos, center, vup;
  center = bounds.center().toPoint3();

  if (ref < 0)
  {
    pos = center + 2.1 * bounds.size().toPoint3();
    vup = Point3d(0, 0, 1);
  }
  else
  {
    const std::vector<Point3d> Axis = { Point3d(1,0,0),Point3d(0,1,0),Point3d(0,0,1) };
    pos = center + 2.1f * bounds.maxsize() * Axis[ref];
    vup = std::vector<Point3d>({ Point3d(0,0,1),Point3d(0,0,1),Point3d(0,1,0) })[ref];
  }

  beginUpdate(
    StringTree("guessPosition").write("bounds", bounds.toString()).write("ref", cstring(ref)),
    Transaction());
  {
    setBounds(bounds);
    setUseOrthoProjection(false);
    setLookAt(pos, center, vup, Quaternion());
    if (!ortho_params_fixed)
      setOrthoParams(guessOrthoParams());
  }
  endUpdate();

  return true;
}


//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::setBounds(BoxNd new_value)
{
  new_value.setPointDim(3);

  auto old_value = this->bounds;
  if (old_value == new_value)
    return;

  beginUpdate(
    StringTree("set").write("target_id", "bounds").write("value", new_value.toString()),
    StringTree("set").write("target_id", "bounds").write("value", old_value.toString()));
  {
    this->bounds = new_value;
    if (!ortho_params_fixed)
      setOrthoParams(guessOrthoParams());
  }
  endUpdate();
}


//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::setLookAt(Point3d pos, Point3d center, Point3d vup, Quaternion quaternion)
{
  beginUpdate(
    StringTree("setLookAt").write("pos", pos.toString()).write("center", center.toString()).write("vup", vup.toString()).write("quaternion", quaternion.toString()),
    StringTree("setLookAt").write("pos", this->pos.toString()).write("center", this->center.toString()).write("vup", this->vup.toString()).write("quaternion", this->quaternion.toString()));
  {
    this->pos=pos;
    this->vup= vup;
    this->center = center;
    this->quaternion= Quaternion();

    if (!ortho_params_fixed)
      setOrthoParams(guessOrthoParams());
  }
  endUpdate();
}


//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::walk(Point3d direction)
{
  auto dir = (this->center - this->pos).normalized();
  auto right = dir.cross(vup).normalized();
  auto up = right.cross(dir);

  auto vt = direction.x * right +
    direction.y * up +
    direction.z * dir;

  beginUpdate(
    StringTree("walk").write("direction", (+direction).toString()),
    StringTree("walk").write("direction", (-direction).toString()));
  {
    this->pos += vt;
    this->center += vt;

    if (!ortho_params_fixed)
      setOrthoParams(guessOrthoParams());
  }
  endUpdate();
}


//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::rotate(double angle, Point3d axis)
{
  if (!angle)
    return;

  beginUpdate(
    StringTree("rotate").write("angle", Utils::radiantToDegree(+angle)).write("axis", axis.toString()),
    StringTree("rotate").write("angle", Utils::radiantToDegree(-angle)).write("axis", axis.toString()));
  {
    this->quaternion = Quaternion(axis, angle * rotation_factor) * this->quaternion;
    if (!ortho_params_fixed)
      setOrthoParams(guessOrthoParams());
  }
  endUpdate();
}

//////////////////////////////////////////////////////////////////////
Frustum GLLookAtCamera::getFinalFrustum() const
{
  Frustum ret;
  ret.setViewport(viewport);
  ret.loadModelview(Matrix::lookAt(this->pos,this->center,this->vup));
  ret.multModelview(Matrix::rotateAroundCenter(center,quaternion.getAxis(),quaternion.getAngle()));
  ret.loadProjection(ortho_params.getProjectionMatrix(use_ortho_projection));
  return ret;
}



//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::writeTo(StringTree& out) const
{
  GLCamera::writeTo(out);

  out.writeValue("bounds", bounds.toString(/*bInterleave*/false));

  out.writeValue("pos",    pos.toString());
  out.writeValue("center", center.toString());
  out.writeValue("vup",    vup.toString());

  out.writeValue("quaternion", quaternion.toString());

  out.writeValue("ortho_params", ortho_params.toString());
  out.writeValue("ortho_params_fixed", cstring(ortho_params_fixed));

  out.writeValue("disable_rotation", cstring(disable_rotation));
  out.writeValue("use_ortho_projection", cstring(use_ortho_projection));
  out.writeValue("rotation_factor", cstring(rotation_factor));
  out.writeValue("pan_factor", cstring(pan_factor));
}

//////////////////////////////////////////////////////////////////////
void GLLookAtCamera::readFrom(StringTree& in) 
{
  GLCamera::readFrom(in);

  this->bounds = BoxNd::fromString(in.readValue("bounds"),/*bInterleave*/false);
  this->bounds.setPointDim(3);

  this->pos = Point3d::fromString(in.readValue("pos"));
  this->center = Point3d::fromString(in.readValue("center"));
  this->vup = Point3d::fromString(in.readValue("vup"));

  this->quaternion = Quaternion::fromString(in.readValue("quaternion"));

  this->ortho_params = GLOrthoParams::fromString(in.readValue("ortho_params", this->ortho_params.toString()));
  this->ortho_params_fixed = in.readBool("ortho_params_fixed", this->ortho_params_fixed);
  
  this->disable_rotation = in.readBool("disable_rotation",this->disable_rotation);
  this->use_ortho_projection = in.readBool("use_ortho_projection",this->use_ortho_projection);
  this->rotation_factor = in.readDouble("rotation_factor", this->rotation_factor);
  this->pan_factor = in.readDouble("pan_factor", this->pan_factor);

}

} //namespace Visus
