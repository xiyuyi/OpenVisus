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

#include <Visus/IdxDataset.h>
#include <Visus/ApplicationInfo.h>

#ifdef WIN32
#pragma warning(disable:4996)
#endif

using namespace Visus;

/**
  This application executes a remote (or local) box query
  and dumps on disk a raw file containing the data.
**/
int main(int argc, const char* argv[])
{

  SetCommandLine(argc,argv);
  IdxModule::attach();

  DoAtExit do_at_exit([]{
    IdxModule::detach();
  });

  auto args=ApplicationInfo::args;

  String dataset_url = "http://atlantis.sci.utah.edu/mod_visus?dataset=2kbit1";
  String fieldname = "";
  String outname = "dump";

  Point3i p1_in, p2_in;
  int end_resolution = -1;

  double timestate = 0;

  for (int I=1;I<(int)args.size();)
  {
    String cmd=StringUtils::toLower(args[I++]);

    if (StringUtils::startsWith(cmd,"--"))
      cmd=cmd.substr(2);

    if(cmd=="dataset")
      dataset_url = args[I];

    else if (cmd == "p1")
      p1_in = Point3i(args[I]);

    else if(cmd=="p2")
      p2_in= Point3i(args[I]);

    else if(cmd=="time")
      timestate = cdouble(args[I]);

    else if(cmd=="field")
      fieldname = args[I];

    else if(cmd=="res")
      end_resolution = cint(args[I]);

    else if(cmd=="out")
      outname = args[I];

    else if(cmd=="help")
    {
      VisusInfo() << " example_query " << std::endl
      << "   [--dataset dataset_url]" << std::endl
      << "   [--field <string>]" << std::endl
      << "   [--res <int>]" << std::endl
      << "   [--p1 XXxYYxZZ]" << std::endl
      << "   [--p2 XXxYYxZZ]" << std::endl
      << "   [--time <int>]" << std::endl
      << "   [--out <string>]" << std::endl;
      return 0;
    }
  }

  auto dataset=LoadDataset(dataset_url);
  VisusReleaseAssert(dataset);

  auto world_box=dataset->getLogicBox();
  auto max_resolution = dataset->getMaxResolution();

  if(end_resolution ==-1) 
    end_resolution =max_resolution;

  VisusInfo() << "Data size: " << world_box.p1.toString() << " " << world_box.p2.toString() << " max res: " << end_resolution;

  //any time you need to read/write data from/to a Dataset create an Access
  auto access=dataset->createAccess();

  auto query=std::make_shared<Query>(dataset.get(),'r');

  BoxNi my_box(p1_in, p2_in);

  VisusInfo() << "Box query " << my_box.p1.toString() << " p2 " << my_box.p2.toString() << " variable " << fieldname << " time " << timestate;

  query->logic_position = my_box;
  query->field = !fieldname.empty()? dataset->getFieldByName(fieldname) : dataset->getDefaultField();

  query->time = timestate;

  // Set resolution levels
  // You can add multiple resolutions values to end_resolutions
  query->start_resolution = 0;
  query->end_resolutions.push_back(end_resolution);

  // In case you use lower resolutions you might want to set a merge_mode
  query->merge_mode = Query::InterpolateSamples;

  VisusReleaseAssert(dataset->nextQuery(query));
  while (true)
  {
    // Read the data
    VisusReleaseAssert(dataset->executeQuery(access, query));

    if (!dataset->nextQuery(query))
      break;
  }

  Array data = query->buffer;

  // Here we dump the data on disk in a .raw file
  VisusInfo() << "Dumping data bytes " << data.c_size();

  std::stringstream ss;
  ss << outname << "_" << query->buffer.getWidth() << "_" << query->buffer.getHeight() << "_" << query->buffer.getDepth() << "_" << query->field.dtype.toString() << ".raw";
  std::ofstream out_file;
  out_file.open(ss.str(), std::ios::out | std::ios::binary);
  out_file.write( (char*)data.c_ptr(), data.c_size());
  out_file.close();

  return 0;
}

