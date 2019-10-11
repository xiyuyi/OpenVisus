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

#include <Visus/TimeNode.h>

namespace Visus {


//////////////////////////////////////////////////
TimeNode::TimeNode(String name,double current_time_,DatasetTimesteps timesteps_) 
  : Node(name),current_time(current_time_),timesteps(timesteps_),play_msec(1000)
{
  this->user_range=timesteps.getRange();
  addOutputPort("time");
}

//////////////////////////////////////////////////
TimeNode::~TimeNode()
{
}


//////////////////////////////////////////////////////////////////////////
void TimeNode::execute(Archive& ar)
{
  if (ar.name == "set")
  {
    String target_id;
    ar.read("target_id", target_id);

    if (target_id == "current_time") {
      double value=0.0;
      ar.read("value", value);
      setCurrentTime(value);
      return;
    }

    if (target_id == "user_range") {
      Range value;
      ar.read("value", value);
      setUserRange(value);
      return;
    }

    if (target_id == "play_msec") {
      int value= 1000;
      ar.read("value", value);
      setPlayMsec(value);
      return;
    }
  }

  return Node::execute(ar);
}

//////////////////////////////////////////////////
void TimeNode::setCurrentTime(double value,bool bDoPublish)
{
  //NOTE: I accept even value if not in timesteps...
  if (this->current_time!=value)
  {
    setProperty("current_time", this->current_time, value);

    if (bDoPublish)
      doPublish();
  }
}

//////////////////////////////////////////////////
void TimeNode::setUserRange(const Range& value)
{
  if (this->user_range==value)
    return;

  setProperty("user_range", this->user_range, value);
  doPublish();
}

//////////////////////////////////////////////////
void TimeNode::setPlayMsec(int value)
{
  if (this->play_msec==value) return;
  setProperty("play_msec", this->play_msec, value);
  //doPublish();
}

//////////////////////////////////////////////////
void TimeNode::enterInDataflow() 
{
  Node::enterInDataflow();
  doPublish();
}

//////////////////////////////////////////////////
void TimeNode::exitFromDataflow() 
{
  Node::exitFromDataflow();
}

//////////////////////////////////////////////////
void TimeNode::doPublish(SharedPtr<ReturnReceipt> return_receipt)
{
  if (!getDataflow()) 
    return;
  
  DataflowMessage msg;
  msg.setReturnReceipt(return_receipt);
  msg.writeValue("time",current_time);
  this->publish(msg);
}

//////////////////////////////////////////////////
void TimeNode::write(Archive& ar) const
{
  Node::write(ar);

  ar.write("current_time", current_time);
  ar.write("user_range", user_range);
  ar.write("play_msec", play_msec);

  ar.writeObject("timesteps", timesteps);
}

//////////////////////////////////////////////////
void TimeNode::read(Archive& ar)
{
  Node::read(ar);

  ar.read("current_time", current_time);
  ar.read("user_range", user_range);
  ar.read("play_msec", play_msec);

  ar.readObject("timesteps", timesteps);
}


} //namespace Visus









