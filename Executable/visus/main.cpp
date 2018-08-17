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

#include <Visus/Dataset.h>
#include <Visus/Access.h>
#include <Visus/IdxFile.h>
#include <Visus/DatasetFilter.h>
#include <Visus/CloudStorage.h>
#include <Visus/IdxDataset.h>
#include <Visus/File.h>
#include <Visus/Encoders.h>
#include <Visus/Idx.h>
#include <Visus/Array.h>
#include <Visus/ModVisus.h>
#include <Visus/Path.h>
#include <Visus/ThreadPool.h>
#include <Visus/NetService.h>
#include <Visus/Utils.h>
#include <Visus/ApplicationInfo.h>
#include <Visus/IdxDiskAccess.h>
#include <Visus/IdxMosaicAccess.h>
#include <Visus/IdxMultipleDataset.h>
#include <Visus/MultiplexAccess.h>

#include <Visus/PythonEngine.h>
#include <pydebug.h>

using namespace Visus;

void execTestIdx(int max_seconds);

///////////////////////////////////////////////////////////
class ConvertStep
{
public:


  //destructor
  virtual ~ConvertStep()
  {}

  //getHelp
  virtual String getHelp()=0;

  //exec
  virtual Array exec(Array data,std::vector<String> args)=0;


};

///////////////////////////////////////////////////////////
class CreateIdx : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <filename.idx>" << std::endl
      << "   [--box <NdBox>]" << std::endl
      << "   [--fields <string>]" << std::endl
      << "   [--bitmask <string>]" << std::endl
      << "   [--bitsperblock <int>]" << std::endl
      << "   [--blocksperfile <int>]" << std::endl
      << "   [--filename_template <string>]" << std::endl
      << "   [--time from to template]" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() < 2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String filename = args[1];

    IdxFile idxfile;

    for (int I = 2; I < (int)args.size(); I++)
    {
      if (args[I] == "--box")
      {
        auto sbox = args[++I];
        int pdim = (int)StringUtils::split(sbox, " ", true).size() / 2; VisusAssert(pdim > 0);
        idxfile.box = NdBox::parseFromOldFormatString(pdim, sbox);
      }

      else if (args[I] == "--fields")
        idxfile.fields = IdxFile::parseFields(args[++I]);

      else if (args[I] == "--bits" || args[I] == "--bitmask_pattern" || args[I] == "--bitmask")
        idxfile.bitmask = DatasetBitmask(args[++I]);

      else if (args[I] == "--bitsperblock")
        idxfile.bitsperblock = cint(args[++I]);

      else if (args[I] == "--blocksperfile")
        idxfile.blocksperfile = cint(args[++I]);

      else if (args[I] == "--filename_template")
        idxfile.filename_template = args[++I];

      else if (args[I] == "--time")
      {
        idxfile.timesteps = DatasetTimesteps();
        String from = args[++I];
        String to = args[++I];
        String templ = args[++I];

        if (from == "*" && to == "*")
          idxfile.timesteps = DatasetTimesteps::star();
        else
          idxfile.timesteps.addTimesteps(cdouble(from), cdouble(to), 1.0);

        idxfile.time_template = templ;
      }
      else
      {
        //just ignore
      }
    }

    if (!IdxDataset::create(filename, data, idxfile))
    {
      VisusError() << "IdxDataset::create(" << filename << ") failed";
      return Array();
    }

    return data;
  }
};

///////////////////////////////////////////////////////////////////////
class StartVisusServer : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " [--port value]" << std::endl
      << "Example: " << " --port 10000 ";
    return out.str();
  }

  //exec
  virtual Array exec(Array data,std::vector<String> args) override
  {
    int    port=10000;

    for (int I=1;I<(int)args.size();I++)
    {
      if (args[I]=="--port" || args[I]=="-p") 
      {
        port=cint(args[++I]);
        continue;
      }
      
      ThrowException(StringUtils::format() << args[0] <<"  Invalid argument " << args[I]);
    }

    auto modvisus=std::make_shared<ModVisus>();
    modvisus->configureDatasets();
  
    auto netserver=std::make_shared<NetServer>(port, modvisus);
    netserver->runInThisThread();

    return data;
  }

};

///////////////////////////////////////////////////////////
class ApplyFilters : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <filename>" << std::endl
      << "   [--window-size <int>]" << std::endl
      << "   [--field <field>]" << std::endl
      << "   [--time <time>]" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size()<2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String filename = args[1];

    auto dataset = Dataset::loadDataset(filename);
    if (!dataset)
      ThrowException(StringUtils::format() << args[0] <<"  Dataset::loadDataset(" << filename << ") failed");

    int window_size = 4096;
    double time = dataset->getDefaultTime();
    Field field = dataset->getDefaultField();

    int pdim = dataset->getPointDim();
    NdPoint sliding_box = NdPoint::one(pdim);

    for (int I = 2; I<(int)args.size(); I++)
    {
      if (args[I] == "--window-size")
      {
        window_size = cint(args[++I]);

        if (!Utils::isPowerOf2(window_size))
          ThrowException(StringUtils::format() << args[0] <<" window size must be power of two");

        continue;
      }

      if (args[I] == "--field")
      {
        String fieldname = args[++I];
        field = dataset->getFieldByName(fieldname);
        if (!field.valid())
          ThrowException(StringUtils::format() << args[0] <<"  Invalid --field " << fieldname);

        continue;
      }

      if (args[I] == "--time")
      {
        time = cdouble(args[++I]);
        continue;
      }
      
      ThrowException(StringUtils::format() << args[0] <<"  unknown argument: " << args[I]);
    }

    //the filter will be applied using this sliding window
    for (int D = 0; D<dataset->getPointDim(); D++)
      sliding_box[D] = window_size;

    auto access = dataset->createAccess();
    auto filter = dataset->createQueryFilter(field);
    VisusInfo() << "starting conversion...";
    filter->computeFilter(time, field, access, sliding_box);
    return data;
  }
};

///////////////////////////////////////////////////////////
class CopyDataset : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <filename.xml> OR" << std::endl
      << " <src_dataset> <dst_dataset>" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
   if (args.size() != 3)
     ThrowException(StringUtils::format() << args[0]<< " syntax error, needed 2 arguments");


    String Surl = args[1]; auto Svf = Dataset::loadDataset(Surl); 
    String Durl = args[2]; auto Dvf = Dataset::loadDataset(Durl); 

    if (!Svf)  
      ThrowException(StringUtils::format() << args[0] <<"  Dataset::loadDataset(" << Surl << ") failed");

    if (!Dvf)  
      ThrowException(StringUtils::format() << args[0] <<"  Dataset::loadDataset(" << Durl << ") failed");

    if (Svf->getTimesteps() != Dvf->getTimesteps())
      ThrowException(StringUtils::format() << args[0] <<" Time range not compatible");

    std::vector<double> timesteps = Svf->getTimesteps().asVector();

    std::vector<Field> Sfields = Svf->getFields();
    std::vector<Field> Dfields = Dvf->getFields();

    if (Sfields.size() != Dfields.size())
      ThrowException(StringUtils::format() << args[0] <<" Fieldnames not compatible");

    auto Saccess = Svf->createAccessForBlockQuery();
    auto Daccess = Dvf->createAccessForBlockQuery();

    for (int time_id = 0; time_id<(int)timesteps.size(); time_id++)
    {
      double timestep = timesteps[time_id];
      for (int F = 0; F<(int)Sfields.size(); F++)
      {
        Dataset::copyDataset(
          Dvf.get(), Daccess, Dfields[F], timestep,
          Svf.get(), Saccess, Sfields[F], timestep);
      }
    }

    return data;
  }
};

///////////////////////////////////////////////////////////
class CompressDataset : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out << " <dataset.xml> <compression>" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 3)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, needed 3 arguments, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String url = args[1]; auto dataset = Dataset::loadDataset(url); 
    if (!dataset)  
      ThrowException(StringUtils::format() << args[0] <<"  Dataset::loadDataset(" << url << ") failed");

    String compression = args[2];
    if (!Encoders::getSingleton()->getEncoder(compression))
      ThrowException(StringUtils::format() << args[0] <<"  encoder(" << compression << ") does not exists");

    if (!dataset->compress(compression))
      ThrowException(StringUtils::format() << args[0] <<" Compression failed");

    return data;
  }
};

///////////////////////////////////////////////////////////
class FixDatasetRange : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <filename> " << std::endl
      << "   [--field <field>]" << std::endl
      << "   [--time <double>]" << std::endl
      << "   [--from <block_from>]" << std::endl
      << "   [--to <block_to>]" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size()<2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, needed filename, got the following arguments:\n " << StringUtils::join(args,"\n"));

    VisusInfo() << "FixDatasetRange starting...";

    String filename = args[1];
    auto vf = IdxDataset::loadDataset(filename);
    if (!vf)
      ThrowException(StringUtils::format() << args[0] <<"  failed to read IDX dataset (Dataset::loadDataset(" << filename << ") failed)");

    auto idxfile = vf->idxfile;

    int maxh = vf->getMaxResolution();
    HzOrder hzorder(idxfile.bitmask, maxh);
    BigInt last_block = (hzorder.getAddress(hzorder.getLevelP2Included(maxh)) >> idxfile.bitsperblock) + 1;
    int    samplesperblock = 1 << idxfile.bitsperblock;

    auto access = vf->createAccessForBlockQuery();
    if (!access)
      ThrowException(StringUtils::format() << args[0] << " cannot create access (vf->createAccess() failed)");

    bool       filter_field = false; String field_filtered = "";
    bool       filter_time = false; double time_filtered = 0;
    BigInt     block_from = 0;
    BigInt     block_to = last_block;
    for (int I = 2; I<(int)args.size(); I++)
    {
      if (args[I] == "--field") { filter_field = true; field_filtered = (args[++I]); }
      else if (args[I] == "--time") { filter_time = true; time_filtered = cdouble(args[++I]); }
      else if (args[I] == "--from") { block_from = cint64(args[++I]); }
      else if (args[I] == "--to") { block_to = cint64(args[++I]); }
    }

    VisusInfo() << "Calculating minmax for " << (filter_field ? "field(" + field_filtered + ")" : "all fields");
    VisusInfo() << "Calculating minmax for " << (filter_time ? "time(" + cstring(time_filtered) + ")" : "all timesteps");
    VisusInfo() << "Calculating minmax in the block range [" << block_from << "," << block_to << ")";

    Time t1 = Time::now();

    Aborted aborted;

    access->beginRead();

    for (int F = 0; F<(int)idxfile.fields.size(); F++)
    {
      Field& field = idxfile.fields[F];

      if (filter_field && field.name != field_filtered)
      {
        VisusInfo() << "ignoring field(" << field.name << "), does not match with --field argument";
        continue;
      }

      //trivial case... don't want to read all the dataset!
      DType dtype = field.dtype;
      if (dtype.isVectorOf(DTypes::INT8))
      {
        VisusInfo() << "range for field(" << field.name << ") of type 'int8' quickly guessed (skipped the reading from disk)";
        continue;
      }

      if (dtype.isVectorOf(DTypes::UINT8))
      {
        VisusInfo() << "range for field(" << field.name << ") of type 'uint8' quickly guessed (skipped the reading from disk)";
        continue;
      }

      for (int C = 0; C<field.dtype.ncomponents(); C++)
      {
        auto sub = field.dtype.get(C);
        Range range = Range::invalid();

        if (sub == DTypes::INT8)
          range = Range(-128, 127,1);

        else if (sub == DTypes::UINT8)
          range = Range(0, 255,1);

        field.setDTypeRange(range, C);
      }

      std::vector<double> timesteps = vf->getTimesteps().asVector();
      for (int time_id = 0; time_id<(int)timesteps.size(); time_id++)
      {
        double time = timesteps[time_id];

        if (filter_time && time != time_filtered)
        {
          VisusInfo() << "ignoring timestep(" << time << ") ,does not match with --time argument";
          continue;
        }

        for (BigInt nblock = block_from; nblock<block_to; nblock++)
        {
          auto read_block = std::make_shared<BlockQuery>(field, time, access->getStartAddress(nblock), access->getEndAddress(nblock), aborted);
          if (!vf->readBlockAndWait(access, read_block))
            continue;

          //need to calculate since I already know it's invalid!
          for (int C = 0; C<field.dtype.ncomponents(); C++)
          {
            auto sub = field.dtype.get(C);

            if (sub == DTypes::INT8 || sub == DTypes::UINT8)
              continue;

            auto range = field.dtype.getDTypeRange(C);
            range = range.getUnion(ArrayUtils::computeRange(read_block->buffer, C));
            field.setDTypeRange(range, C);
          }

          //estimation
          if (t1.elapsedMsec()>5000)
          {
            VisusInfo()
              << "RANGE time(" << time << ")"
              << " field(" << field.name << ")"
              << " nblock/from/to(" << nblock << "/" << block_from << "/" << block_to << ")";

            for (int C = 0; C<field.dtype.ncomponents(); C++)
              VisusInfo() << "  what(" << C << ") " << field.dtype.getDTypeRange(C).toString();

            idxfile.fields[F] = field;

            //try to save the intermediate file (NOTE: internally using locks so it should be fine to run in parallel)
            if (!idxfile.save(filename))
            {
              VisusWarning() << "cannot save the INTERMEDIATE min-max in IDX dataset (vf->idxfile.save(" << filename << ") failed)";
              VisusWarning() << "Continuing anyway hoping to solve the problem saving the file later";
            }

            t1 = Time::now();
          }
        }
      }

      VisusInfo() << "done minmax for field(" << field.name << ")";

      for (int C = 0; C<field.dtype.ncomponents(); C++)
        VisusInfo() << "  what(" << C << ") " << field.dtype.getDTypeRange(C).toString();
    }

    access->endRead();

    //finally save the file
    if (!idxfile.save(filename))
      ThrowException(StringUtils::format() << args[0] <<"  cannot save the FINAL min-max in IDX dataset (IdxFile::save(" << filename << ") failed)");

    VisusInfo() << "done fixFieldsRange";

    for (auto field : vf->getFields())
      VisusInfo() << field.toString();

    return data;
  }
};

///////////////////////////////////////////////////////////////////////////////
class ConvertMidxToIdx : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out
      << " <midx_filename> <idx_filename> [--tile-size value] [--field value]" << std::endl
      << "Example: " << " D:/Google Drive sci.utah.edu/visus_dataset/slam/Alfalfa/visus.midx D:/Google Drive sci.utah.edu/visus_dataset/slam/Alfalfa/visus.idx";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    srand(0);

    String midx_filename = args[1];
    String idx_filename = args[2];
    int TileSize = 4 * 1024;
    String fieldname = "output=voronoiBlend()";

    for (int I = 1; I < args.size(); I++)
    {
      if (args[I] == "--tile-size")
        TileSize = cint(args[++I]);

      else if (args[I] == "--field")
        fieldname = args[++I];
    }

    auto midx = std::dynamic_pointer_cast<IdxMultipleDataset>(Dataset::loadDataset(midx_filename)); VisusReleaseAssert(midx);
    auto midx_access = midx->createAccess();

    bool bOk = midx->createIdxFile(idx_filename, Field("DATA", midx->getFieldByName(fieldname).dtype, "rowmajor"));
    VisusReleaseAssert(bOk);

    auto idx = IdxDataset::loadDataset(idx_filename);
    auto idx_access = idx->createAccess();

    auto tiles = midx->generateTiles(TileSize);

    auto T1 = Time::now();
    for (int TileId = 0; TileId<tiles.size(); TileId++)
    {
      auto tile = tiles[TileId];

      auto t1 = Time::now();
      auto buffer = midx->readMaxResolutionData(midx_access, tile);
      int msec_read = (int)t1.elapsedMsec();
      if (!buffer)
        continue;

      t1 = Time::now();
      idx->writeMaxResolutionData(idx_access, tile, buffer);
      int msec_write = (int)t1.elapsedMsec();

      VisusInfo() << "done " << TileId << " of " << tiles.size() << " msec_read(" << msec_read << ") msec_write(" << msec_write << ")";

      //ArrayUtils::saveImage(StringUtils::format() << "tile_" << TileId << ".png", read->buffer);
    }

    VisusInfo() << "ALL DONE IN " << T1.elapsedMsec();
    return data;
  }

};


///////////////////////////////////////////////////////////
class ImportData : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <filename> " << std::endl
      << "   [load_args]*" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size()<2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, needed filename, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String filename = args[1];

    auto ret = ArrayUtils::loadImage(filename, args);
    if (!ret)
      ThrowException(StringUtils::format() << args[0] <<"  cannot load image " << filename);

    return ret;
  }
};

///////////////////////////////////////////////////////////
class ExportData : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out <<" <filename> [save_args]*" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size()<2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String filename = args[1];

    if (!ArrayUtils::saveImage(filename, data, args))
      ThrowException(StringUtils::format() << args[0] <<"  saveImage failed " << filename);

    return data;
  }
};

///////////////////////////////////////////////////////////
class PasteData : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <filename> " << std::endl
      << "   [--source-box      <NdBox>]" << std::endl
      << "   [--destination-box <NdBox>]" << std::endl
      << "   [load_args]*" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size()<2)
      ThrowException(StringUtils::format() << args[0]<< " syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String filename = args[1];

    Array to_paste = ArrayUtils::loadImage(filename, args);
    if (!to_paste)
      ThrowException(StringUtils::format() << args[0] <<"  Cannot load " << filename);

    int pdim = data.getPointDim();

    //embedding in case I'm missing point-dims (see interleaving)
    if (pdim>to_paste.dims.getPointDim())
      to_paste.dims.setPointDim(pdim);

    NdBox Dbox(NdPoint(pdim), data.dims);
    NdBox Sbox(NdPoint(pdim), to_paste.dims);
    for (int I = 2; I<(int)args.size(); I++)
    {
      if (args[I] == "--destination-box")
        Dbox = NdBox::parseFromOldFormatString(pdim,args[++I]);

      else if (args[I] == "--source-box") {
        Sbox = NdBox::parseFromOldFormatString(pdim, args[++I]);
        if (pdim > Sbox.getPointDim()) {
          Sbox.p1.setPointDim(pdim);
          Sbox.p2.setPointDim(pdim);
        }
      }
    }

    if (!ArrayUtils::paste(data, Dbox, to_paste, Sbox))
      ThrowException(StringUtils::format() << args[0] <<" paste of image failed");

    return data;
  }
};

///////////////////////////////////////////////////////////
class GetComponent : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <expression>" << std::endl
      << "Example: " << " 0";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] << " syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    int C = cint(args[1]);
    return data.getComponent(C);
  }
};

///////////////////////////////////////////////////////////
class Cast : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out <<" <dtype>" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] << " syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    DType dtype = DType::fromString(args[1]);
    return ArrayUtils::cast(data, dtype);
  }
};

///////////////////////////////////////////////////////////
class SmartCast : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out <<" <dtype>" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] << " syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    DType dtype = DType::fromString(args[1]);
    return ArrayUtils::smartCast(data, dtype);
  }
};

///////////////////////////////////////////////////////////
class ResizeData : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << "   [--dtype <DType>]" << std::endl
      << "   [--dims <NdPoint>]" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size()<2)
      ThrowException(StringUtils::format() << args[0] << " syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    DType   dtype = data.dtype;
    NdPoint dims = data.dims;
    for (int I = 1; I<(int)args.size(); I++)
    {
      if (args[I] == "--dtype")
      {
        dtype = DType::fromString(args[++I]);
        continue;
      }

      if (args[I] == "--dims")
      {
        dims = NdPoint::parseDims(args[++I]);
        continue;
      }
      
      ThrowException(StringUtils::format() << args[0] << " Invalid arguments " << args[I]);
    }

    if (!data.resize(dims, dtype, __FILE__, __LINE__))
      ThrowException(StringUtils::format() << args[0] << " resize failed");

    return data;
  }
};

///////////////////////////////////////////////////////////
class CropData : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out << " <NdBox>" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] << " syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    int pdim = data.getPointDim();
    String sbox = args[1];

    NdBox box = NdBox::parseFromOldFormatString(pdim,sbox);
    if (!box.isFullDim())
      ThrowException(StringUtils::format() << args[0] <<"  Invalid box " << sbox);

    return ArrayUtils::crop(data, box);
  }
};

///////////////////////////////////////////////////////////
class MirrorData : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out << " <int>" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    int axis = cint(args[1]);
    if (axis<0)
      ThrowException(StringUtils::format() << args[0] <<"  Invalid axis " << args[1]);

    return ArrayUtils::mirror(data, axis);
  }
};

///////////////////////////////////////////////////////////
class ComputeComponentRange : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <expression>" << std::endl
      << "Example: " << " 0";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    int C = cint(args[1]);
    Range range = ArrayUtils::computeRange(data, C);
    VisusInfo() << "Range of component " << C << " is " << range.toString();

    return data;
  }
};

///////////////////////////////////////////////////////////
class InterleaveData : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 1)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    if (data.dtype.ncomponents()>1 && !Utils::isByteAligned(data.dtype.get(0).getBitSize()))
      ThrowException(StringUtils::format() << args[0] <<" request to --interleave but a sample is not byte aligned");

    //need to interleave (the input for example is RRRRR...GGGGG...BBBBB and I really need RGBRGBRGB)
    int ncomponents = data.dtype.ncomponents();
    if (ncomponents <= 1)
      return data;

    std::vector<Array> v;
    for (int I = 0; I<ncomponents; I++)
    {
      DType component_dtype = data.dtype.get(I);
      Int64 offset = I*component_dtype.getByteSize(data.dims);
      v.push_back(Array::createView(data, data.dims, component_dtype, offset));
    }

    return ArrayUtils::interleave(v);
  }
};

///////////////////////////////////////////////////////////
class PrintInfo : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out << " <filename>";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String filename = args[1];
    StringTree info = ArrayUtils::statImage(filename);
    if (info.empty())
      ThrowException(StringUtils::format() << args[0] <<"  Could not open " << filename);

    VisusInfo() << std::endl << info.toString();

    auto dataset = Dataset::loadDataset(filename);
    return data;
  }
};

///////////////////////////////////////////////////////////
class DumpData : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 1)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    VisusInfo() << "Buffer dims(" << data.dims.toString() << ") dtype(" << data.dtype.toString() << ")";

    Uint8* SRC = data.c_ptr();
    Int64 N = data.c_size();
    for (int I = 0; I<N; I++)
    {
      std::cout << std::setfill('0') << std::hex << "0x" << std::setw(2) << (int)(*SRC++);
      if (I != N - 1) std::cout << ",";
      if ((I % 16) == 15) std::cout << std::endl;
    }
    std::cout << std::endl;
    std::cout << std::endl;
    return data;
  }
};

///////////////////////////////////////////////////////////
class ReadWriteBlock : public ConvertStep
{
public:

  //constructor
  ReadWriteBlock(bool bWriting_) : bWriting(bWriting_) {
  }
  
  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <filename>" << std::endl
      << "   [--block <int>]" << std::endl
      << "   [--field <field>]" << std::endl
      << "   [--time <time>]" << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size()<2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String url = args[1];

    auto dataset = Dataset::loadDataset(url);
    if (!dataset)
      ThrowException(StringUtils::format() << args[0] <<"  Dataset::loadDataset(" << url << ") failed");

    BigInt block_id = 0;
    Field  field = dataset->getDefaultField();
    double time = dataset->getDefaultTime();

    for (int I = 2; I<(int)args.size(); I++)
    {
      if (args[I] == "--block")
      {
        block_id = cbigint(args[++I]);
        continue;
      }

      if (args[I] == "--field")
      {
        String fieldname = args[++I];
        field = dataset->getFieldByName(fieldname);
        if (!field.valid())
          ThrowException(StringUtils::format() << args[0] <<"  specified field (" << fieldname << ") is wrong");
        continue;
      }

      if (args[I] == "--time")
      {
        double time = cdouble(args[++I]);
        if (!dataset->getTimesteps().containsTimestep(time))
          ThrowException(StringUtils::format() << args[0] <<"  specified time (" << time << ") is wrong");
        continue;
      }

      ThrowException(StringUtils::format() << args[0] <<"  Invalid argument " << args[I]);
    }

    VisusInfo() << "url(" << url << ") block(" << block_id << ") field(" << field.name << ") time(" << time << ")";

    auto access=dataset->createAccessForBlockQuery();

    auto block_query = std::make_shared<BlockQuery>(field, time, block_id *(((Int64)1) << access->bitsperblock), (block_id + 1)*(((Int64)1) << access->bitsperblock), Aborted());
    ApplicationStats::io.readValues(true);

    auto t1 = Time::now();

    Array ret;
    if (bWriting)
    {
      access->beginWrite();
      bool bOk = dataset->writeBlockAndWait(access, block_query);
      access->endWrite();
      if (!bOk)
        ThrowException(StringUtils::format() << args[0] <<" Failed to write block");
      ret=data;
    }
    else
    {
      access->beginRead();
      bool bOk = dataset->readBlockAndWait(access, block_query);
      access->endRead();
      if (!bOk)
        ThrowException(StringUtils::format() << args[0] <<" Failed to write block");
      ret=block_query->buffer;
    }


    auto stats=ApplicationStats::io.readValues(true);
    VisusInfo() << (bWriting?"Wrote":"Read")<< " block("<<block_id<< ") in msec(" << t1.elapsedMsec() << ") nopen(" << stats.nopen << ") rbytes(" << StringUtils::getStringFromByteSize(stats.rbytes) << ") wbytes(" << StringUtils::getStringFromByteSize(stats.wbytes) << ")";
    return ret;
  }

private:

  bool bWriting;
};



///////////////////////////////////////////////////////////
class CloudCreateContainer : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <url>" << std::endl
      << "Example: "  << " http://visus.blob.core.windows.net?password=XXXXX";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String url = args[1];
    UniquePtr<CloudStorage> src_storage(CloudStorage::createInstance(url));

    if (!src_storage->createContainer(url))
      ThrowException(StringUtils::format() << args[0] <<"  cannot create the container " << url);
    
    return data;
  }
};

///////////////////////////////////////////////////////////
class CloudDeleteContainer : public ConvertStep
{
public:

   
  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <url>" << std::endl
      << "Example: " << " http://visus.blob.core.windows.net/2kbit1?password=XXXXX";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String url = args[1];
    UniquePtr<CloudStorage> src_storage(CloudStorage::createInstance(url));
    if (!src_storage->deleteContainer(url))
      ThrowException(StringUtils::format() << args[0] <<"  cannot delete the container " << url);
    return data;
  }
};

///////////////////////////////////////////////////////////
class CloudListContainers : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <filename>" << std::endl
      << "Example: " << " http://visus.blob.core.windows.net?password=XXXXX";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String url = args[1];
    UniquePtr<CloudStorage> src_storage(CloudStorage::createInstance(url));
    StringTree tree = src_storage->listContainers(url);
    VisusInfo() << "\n" << tree.toString();
    return data;
  }
};

///////////////////////////////////////////////////////////
class CloudCopyBlob : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <url> <dst_url>" << std::endl
      << "Example: " << " http://atlantis.sci.utah.edu/mod_visus?readdataset=2kbit1  http://visus.blob.core.windows.net/2kbit1/visus.idx?password=XXXXX";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 3)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String src_url = args[1];
    String dst_url = args[2];

    UniquePtr<CloudStorage> src_storage(CloudStorage::createInstance(src_url));
    UniquePtr<CloudStorage> dst_storage(CloudStorage::createInstance(dst_url));

    StringMap             metadata;
    SharedPtr<HeapMemory> blob;
    if (src_storage)
    {
      blob = src_storage->getBlob(src_url, metadata);
      if (!blob)
        ThrowException(StringUtils::format() << args[0] <<"  src_storage->getBlob(" << src_url << ") failed");
    }
    else
    {
      blob = Utils::loadBinaryDocument(src_url);
      if (!blob)
        ThrowException(StringUtils::format() << args[0] <<"  Utils::loadBinaryDocument(" << src_url << ") failed");
    }

    if (dst_storage)
    {
      if (!dst_storage->addBlob(dst_url, blob, metadata))
        ThrowException(StringUtils::format() << args[0] <<"  dst_storage->addBlob(" << src_url << ") failed");
    }
    else if (!Utils::saveBinaryDocument(dst_url, blob))
    {
      ThrowException(StringUtils::format() << args[0] <<"  Utils::saveBinaryDocument(" << dst_url << ") failed");
    }

    return data;
  }
};

///////////////////////////////////////////////////////////
class CloudListBlobs : public ConvertStep
{
public:
  
  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <url>" << std::endl
      << "Example: " << " http://visus.blob.core.windows.net/2kbit1?password=XXXXX";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String url = args[1];
    UniquePtr<CloudStorage> src_storage(CloudStorage::createInstance(url));
    StringTree tree = src_storage->listOfBlobs(url);
    VisusInfo() << "\n" << tree.toString();
    return data;
  }
};

///////////////////////////////////////////////////////////
class CloudDeleteBlob : public ConvertStep
{
public:
  
  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <url>" << std::endl
      << "Example: " << " http://visus.blob.core.windows.net/2kbit1/visus.idx?password=XXXXX";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 2)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String url = args[1];
    UniquePtr<CloudStorage> src_storage(CloudStorage::createInstance(url));
    if (!src_storage->deleteBlob(url))
      ThrowException(StringUtils::format() << args[0] <<"  cannot delete blob " << url);
    return data;
  }
};

//////////////////////////////////////////////////////////////////////////////
class CloudSelfTest : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out
      << " url" << std::endl
      << "Example: " << " http://visus.s3.amazonaws.com?username=AKIAILLJI7ANO6Y2XJNA&password=XXXXXXXXX"
      << "Example: " << " http://visus.blob.core.windows.net?password=XXXXXXXXXXXX";

    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    Url url(args[1]);

    if (!url.valid())
      ThrowException(StringUtils::format() << args[0] << ", "<< args[1] << " is not a valid url");

    StringMap blob_metadata, check_blob_metadata;
    blob_metadata.setValue("example-meta-data", "visus-meta-data");

    auto blob = Utils::loadBinaryDocument("datasets/cat/gray.png");
    VisusReleaseAssert(blob);

    String container_name = "testing-cloud-storage";

    Url container_url = url; container_url.setPath(url.getPath() + "/" + container_name);
    Url blob_url = container_url;     blob_url.setPath(url.getPath() + "/" + container_name + "/" + "visus.png");

    UniquePtr<CloudStorage> cloud_storage(CloudStorage::createInstance(container_url));
    VisusReleaseAssert(cloud_storage);
    VisusReleaseAssert(cloud_storage->createContainer(container_url));
    VisusReleaseAssert(cloud_storage->addBlob(blob_url, blob, blob_metadata));

    auto check_blob = cloud_storage->getBlob(blob_url, check_blob_metadata);
    VisusReleaseAssert(check_blob);
    VisusReleaseAssert(blob->c_size() == check_blob->c_size() && memcmp(blob->c_ptr(), check_blob->c_ptr(), (size_t)blob->c_size()) == 0);
    VisusReleaseAssert(check_blob_metadata.getValue("example-meta-data") == "visus-meta-data");
    VisusReleaseAssert(cloud_storage->deleteBlob(blob_url));
    VisusReleaseAssert(cloud_storage->deleteContainer(container_url));

    return data;
  }
};



///////////////////////////////////////////////////////////
class TestEncoderSpeed : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " compresion <dataset>" << std::endl
      << "Example: " << " G:/visus_dataset/2kbit1/hzorder/visus.idx zip";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() != 3)
      ThrowException(StringUtils::format() << args[0] <<"  syntax error, got the following arguments:\n " << StringUtils::join(args,"\n"));

    String filename = args[1];
    String compression = args[2];
    auto dataset = Dataset::loadDataset(filename);

    if (!Encoders::getSingleton()->getEncoder(compression))
      ThrowException(StringUtils::format() << args[0] <<"  failed to create encoder " << compression << "");

    if (!dataset)
      ThrowException(StringUtils::format() << args[0] <<"  failed to read dataset (Dataset::loadDataset(" << filename << ") failed)");

    auto access=dataset->createAccessForBlockQuery();
    auto field = dataset->getDefaultField();
    auto time = dataset->getDefaultTime();

    BigInt decoded_bytes = 0; double decode_sec = 0;
    BigInt encoded_bytes = 0; double encode_sec = 0;

    BigInt block = 0;
    BigInt nblocks = dataset->getTotalnumberOfBlocks();

    auto printStatistics = [&]()
    {
      auto encoded_mb = encoded_bytes / (1024 * 1024);
      auto decoded_mb = decoded_bytes / (1024 * 1024);
      auto ratio = 100.0*(encoded_mb / (double)decoded_mb);

      VisusInfo() << "Ratio " << ratio << "% block(" << block << "/" << nblocks << ")";
      VisusInfo() << "Encoding MByte(" << encoded_mb << ") sec(" << encode_sec << ") MByte/sec(" << (encoded_mb / encode_sec) << ")";
      VisusInfo() << "Decoding MByte(" << decoded_mb << ") sec(" << decode_sec << ") MByte/sec(" << (decoded_mb / decode_sec) << ")";
      VisusInfo();
    };

    Time t1 = Time::now();

    Aborted aborted;
    access->beginRead();

    for (block = 0; block<nblocks; block++)
    {
      if (t1.elapsedSec()>5)
      {
        printStatistics();
        t1 = Time::now();
      }

      auto read_block = std::make_shared<BlockQuery>(field, time, access->getStartAddress(block), access->getEndAddress(block), aborted);
      
      if (!dataset->readBlockAndWait(access, read_block))
        continue;

      auto decoded = read_block->buffer;

      SharedPtr<HeapMemory> encoded;

      Time encode_t1 = Time::now();
      {
        encoded = ArrayUtils::encodeArray(compression, decoded);
        if (!encoded)
          ThrowException(StringUtils::format() << args[0] <<" failed to encode array, should not happen");
      }
      encode_sec += encode_t1.elapsedSec();

      Array decoded2;

      Time decode_t1 = Time::now();
      {
        decoded2 = ArrayUtils::decodeArray(compression, decoded.dims, decoded.dtype, encoded);

        if (!decoded2)
          ThrowException(StringUtils::format() << args[0] <<" failed to decode array, should not happen");
      }
      decode_sec += decode_t1.elapsedSec();

      if (decoded.c_size() != decoded2.c_size() ||
        memcmp(decoded.c_ptr(), decoded2.c_ptr(), decoded.c_size()) != 0)
        ThrowException(StringUtils::format() << args[0] <<" encode/decode is not working at all");

      encoded_bytes += encoded->c_size();
      decoded_bytes += decoded.c_size();
    }

    access->endRead();

    printStatistics();

    return data;
  }
};

///////////////////////////////////////////////////////////////////////
class TestQuerySpeed : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " <filename> [--query-dim value]" << std::endl
      << "Example: " << " C:/free/visus_dataset/2kbit1/zip/hzorder/visus.idx --query-dim 512";
    return out.str();
  }
  
  //exec
  virtual Array exec(Array data,std::vector<String> args) override
  {
    if (args.size() < 2)
      ThrowException(StringUtils::format() << args[0] << " syntax error");

    String filename = args[1];
    auto dataset = Dataset::loadDataset(filename);
    if (!dataset)
      ThrowException(StringUtils::format() << args[0] << "  cannot loadDataset " << filename);

    int    query_dim=512;

    for (int I=2;I<(int)args.size();I++)
    {
      if (args[I]=="--query-dim")
      {
        String s_query_dim = args[++I];

        if (!StringUtils::tryParse(s_query_dim,query_dim) || query_dim<=0)
          ThrowException(StringUtils::format() << args[0] <<"  Invalid --query-dim " << s_query_dim);

        continue;
      }

      ThrowException(StringUtils::format() << args[0] <<" Invalid args "<<args[I]);
    }

    srand(0);

    VisusInfo() << "Testing query...";

    auto access = dataset->createAccess();
    auto world_box = dataset->getBox();

    Time T1 = Time::now();
    for (int Q = 0; Q < 10; Q++)
    {
      Time t1 = Time::now();

      auto ndbox = world_box;

      for (int I = 0; I < dataset->getPointDim(); I++)
      {
        ndbox.p1[I] = Utils::getRandInteger(0, (int)world_box.p2[I] - query_dim);
        ndbox.p2[I] = ndbox.p1[I] + query_dim;
      }

      auto query = std::make_shared<Query>(dataset.get(), 'r');
      query->position = ndbox;

      VisusReleaseAssert(dataset->beginQuery(query));
      VisusReleaseAssert(dataset->executeQuery(access, query));

      auto sec = t1.elapsedSec();

      auto stats = access ? access->statistics : Access::Statistics();
      auto io = ApplicationStats::io.readValues(true);

      VisusInfo() << "sec(" << sec << ")"
        << "  box(" << ndbox.toString() << ") "
        << " access.rok(" << stats.rok << "/" << ((double)(stats.rok) / sec) << ") "
        << " access.rfail(" << stats.rfail << "/" << ((double)(stats.rfail) / sec) << ") "
        << " io.nopen(" << io.nopen << "/" << ((double)(io.nopen) / sec) << ") "
        << " io.rbytes(" << double(io.rbytes) / (1024 * 1024) << "/" << double(io.rbytes) / (sec * 1024 * 1024) << ") "
        << " io.wbytes(" << double(io.wbytes) / (1024 * 1024) << "/" << double(io.wbytes) / (sec * 1024 * 1024) << ") ";
    }

    VisusInfo() << "all done in " << T1.elapsedMsec();
    return data;
  }
};

//////////////////////////////////////////////////////////////////////////////
class TestFileReadWriteSpeed : public ConvertStep
{
public:

  bool bWriting;

  //constructor
  TestFileReadWriteSpeed(bool bWriting_) : bWriting(bWriting_) {
  }

  //destructor
  virtual ~TestFileReadWriteSpeed() {
  }

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out << " [--filename value] [--blocksize value] [--filesize value]" << std::endl;
    out << "Example: " << " --filename example.idx --blocksize 64KB --filesize 1MB";
    return out.str();
  }

  //exec
  virtual Array exec(Array data,std::vector<String> args) override
  {
    String filename="temp.bin";
    int blocksize=64*1024;
    int filesize=1*1024*1024*1024;
  
    for (int I=0;I<(int)args.size();I++)
    {
      if (args[I]=="--filename")
        filename=args[++I];

      else if (args[I]=="--blocksize")
        blocksize=(int)StringUtils::getByteSizeFromString(args[++I]);

      else if (args[I]=="--filesize")
        filesize=(int)StringUtils::getByteSizeFromString(args[++I]);
    }

    if (bWriting)
    {
      remove(filename.c_str());

      File file;
      if(!file.createOrTruncateAndWriteBinary(filename))
        ThrowException(StringUtils::format() << args[0] <<" TestWriteIO, file.open"<<filename<<",\"wb\") failed");

      Array blockdata;
      bool bOk=blockdata.resize(blocksize,DTypes::UINT8,__FILE__,__LINE__);
      VisusReleaseAssert(bOk);

      Time t1=Time::now();
      int nwritten;
      for (nwritten=0;(nwritten+blocksize)<=filesize;nwritten+=blocksize)
      {
        if(!file.write(blockdata.c_ptr(),blocksize))
          ThrowException(StringUtils::format() << args[0] <<" TestWriteIO write(...) failed");
      }
      file.close();
      double elapsed=t1.elapsedSec();
      int mbpersec=(int)(nwritten/(1024.0*1024.0*elapsed));
      //do not remove, I can need it for read
      //remove(filename.c_str());
      VisusInfo()<<"write("<<mbpersec<<" mb/sec) filesize("<<filesize<<") blocksize("<<blocksize<<") nwritten("<<nwritten<<") filename("<<filename<<")";
    }
    else
    {
      File file;
      if(!file.openReadBinary(filename))
        ThrowException(StringUtils::format() << args[0] <<" file.openReadBinary("<<filename<<") failed");

      Array blockdata;
      bool bOk=blockdata.resize(blocksize,DTypes::UINT8,__FILE__,__LINE__);
      VisusReleaseAssert(bOk);
      Time t1=Time::now();
      Int64 nread=0;
      int maxcont=1000,cont=maxcont;
      while (true)
      {
        if(!file.read(blockdata.c_ptr(),blocksize)) 
          break;
        nread+=blocksize;


        if (!--cont)
        {
          cont=maxcont;
          VisusInfo()<<"read("<<((nread/(1024*1024))/t1.elapsedSec())<<" mb/sec) blocksize("<<blocksize<<") nread("<<(nread/(1024*1024))<<" mb) filename("<<filename<<")";      
        }
      }
      file.close();
      VisusInfo()<<"read("<<((nread/(1024*1024))/t1.elapsedSec())<<" mb/sec) blocksize("<<blocksize<<") nread("<<(nread/(1024*1024))<<" mb) filename("<<filename<<")";  
    }

    return data;
  }
};

//////////////////////////////////////////////////////////////////////////////
class IdxSelfTest : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out 
      << " [--max-seconds value]" << std::endl
      << "Example: " << " --max-seconds 300";
    return out.str();
  }

  //exec
  virtual Array exec(Array data,std::vector<String> args) override
  {
    int max_seconds=300; 
    for (int I=1;I<(int)args.size();I++)
    {
      if (args[I]=="--max-seconds") 
        max_seconds=cint(args[++I]);
    }

    execTestIdx(max_seconds);
    return data;
  }
};

///////////////////////////////////////////////////////////////////////////////
class TestIdxSlabSpeed : public ConvertStep
{
public:

  //see https://github.com/sci-visus/visus/issues/126
  /* Hana numbers:
  

  dims 512 x 512 x 512
  num_slabs 128
  dtype int32 

  Wrote all slabs in  8.337sec
  */

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out
      << " [--dims       <dimensions>]" << std::endl
      << " [--num-slabs  <value>]" << std::endl
      << " [--dtype      <dtype>]" << std::endl
      << "Example: " << " --dims '512 512 512' --num-writes 128 --dtype int32";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    String filename = "./temp/TestIdxSlabSpeed/visus.idx";

    auto dirname = Path(filename).getParent();
    if (FileUtils::existsDirectory(dirname))
    {
      VisusInfo() << "Please empty the directory " << dirname.toString();
      return Array();
    }

    NdPoint dims            = NdPoint::one(512, 512, 512);
    int     num_slabs       = 128;
    String  dtype           = "int32";

    for (int I = 0; I<(int)args.size(); I++)
    {
      if (args[I] == "--dims")
        dims = NdPoint::parseDims(args[++I]);

      else if (args[I] == "--num-slabs") {
        num_slabs = cint(args[++I]);
        VisusAssert((dims[2] % num_slabs) == 0);
      }

      else if (args[I] == "--dtype")
        dtype = args[++I];
    }

    int slices_per_slab = (int)dims[2] / num_slabs;

    VisusInfo() << "--dims            " << dims.toString();
    VisusInfo() << "--num-slabs       " << num_slabs;
    VisusInfo() << "--dtype           " << dtype;
    VisusInfo() << "--slices-per-slab " << slices_per_slab;

    //create the idx file
    {
      IdxFile idxfile;
      idxfile.box = NdBox(NdPoint(3),dims);
      {
        Field field("myfield", DType::fromString(dtype));
        field.default_compression = "";
        field.default_layout = "1"; //ROW MAJOR as in hana
        idxfile.fields.push_back(field);
      }
      VisusReleaseAssert(idxfile.save(filename));
    }

    //now create a Dataset, save it and reopen from disk
    auto dataset = Dataset::loadDataset(filename);
    VisusReleaseAssert(dataset && dataset->valid());

    //any time you need to read/write data from/to a Dataset I need a Access
    auto access = dataset->createAccess();

    //for example I want to write data by slices
    Int32 sample_id = 0;
    double SEC = 0;

    for (int Slab = 0; Slab < num_slabs; Slab++)
    {
      //this is the bounding box of the region I'm going to write
      auto Z1 = Slab * slices_per_slab;
      auto Z2 =   Z1 + slices_per_slab;

      NdBox slice_box = dataset->getBox().getZSlab(Z1,Z2);

      //prepare the write query
      auto write = std::make_shared<Query>(dataset.get(), 'w');
      write->position = slice_box;
      VisusReleaseAssert(dataset->beginQuery(write));

      int slab_num_samples = (int)(dims[0] * dims[1] * slices_per_slab);
      VisusReleaseAssert(write->nsamples.innerProduct() == slab_num_samples);

      //fill the buffers with some fake data
      {
        Array buffer(write->nsamples, write->field.dtype);

        VisusAssert(dtype == "int32");
        GetSamples<Int32> samples(buffer);

        for (int I = 0; I < slab_num_samples; I++)
          samples[I] = sample_id++;

        write->buffer = buffer;
      }

      //execute the writing
      auto t1 = clock();
      VisusReleaseAssert(dataset->executeQuery(access, write));
      auto t2 = clock();
      auto sec = (t2 - t1) / (float)CLOCKS_PER_SEC;
      SEC += sec;
      VisusInfo() << "Done " << Slab << " of " << num_slabs << " bbox " << slice_box.toString() <<" in "<< sec <<"sec";
    }

    VisusInfo()<<"Wrote all slabs in " << SEC << "sec";

    if (bool bVerify=true)
    {
      auto read = std::make_shared<Query>(dataset.get(), 'r');
      read->position = dataset->getBox();
      VisusReleaseAssert(dataset->beginQuery(read));

      Array buffer(read->nsamples, read->field.dtype);
      buffer.fillWithValue(0);
      read->buffer = buffer;

      auto t1 = clock();
      VisusReleaseAssert(dataset->executeQuery(access, read));
      auto t2 = clock();
      auto sec = (t2 - t1) / (float)CLOCKS_PER_SEC;

      VisusAssert(dtype == "int32");
      GetSamples<Int32> samples(buffer);

      for (int I = 0, N= (int)dims.innerProduct(); I < N; I++)
      {
        if (samples[I] != I)
          VisusInfo() << "Reading verification failed sample "<<I<<" expecting "<< I <<" got "<< samples[I];
      }
    }

    return Array();
  }

};

//////////////////////////////////////////////////////////////////////////////
class TestNetworkSpeed : public ConvertStep
{
public:

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out
      << " [--c nconnections] [--n nrequests] (url)+" << std::endl
      << "Example: " << " --c 8 -n 1000 http://atlantis.sci.utah.edu/mod_visus?from=0&to=65536&dataset=david_subsampled";
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    int nconnections = 1;
    int nrequests = 1;
    std::vector<String> urls;

    for (int I = 1; I < args.size(); I++)
    {
      if (args[I] == "-c")
        nconnections = cint(args[++I]);

      else if (args[I] == "-n")
        nrequests = cint(args[++I]);

      else
        urls.push_back(args[I]);
    }

    VisusInfo() << "Concurrency " << nconnections;
    VisusInfo() << "nrequest " << nrequests;
    VisusInfo() << "urls";
    for (auto url : urls)
      VisusInfo() << "  " << url;

    auto net = std::make_shared<NetService>(nconnections, false);

    Time t1 = Time::now();

    WaitAsync< Future<NetResponse>, int > async;
    for (int Id = 0; Id < nrequests; Id++)
      async.pushRunning(net->asyncNetworkIO(NetRequest(urls[Id % urls.size()])), Id);

    for (int Id = 0; Id < nrequests; Id++)
    {
      if (async.popReady().first.get().status != HttpStatus::STATUS_OK)
        VisusInfo() << "one request failed";

      if (Id && (Id % 100) == 0)
        VisusInfo() << "Done " << Id << " request";
    }

    auto sec = t1.elapsedSec();
    auto stats = ApplicationStats::net.readValues(true);

    VisusInfo() << "All done in " << sec << "sec";
    VisusInfo()
      << " Num request/sec " << double(nrequests) / sec << ") "
      << " read  " << StringUtils::getStringFromByteSize(stats.rbytes) << " bytes/sec " << double(stats.rbytes) / (sec) << ") "
      << " write " << StringUtils::getStringFromByteSize(stats.wbytes) << " bytes/sec " << double(stats.wbytes) / (sec) << ") ";

    return data;
  }
};


using namespace Visus;

//////////////////////////////////////////////////////////////////////////////
class DoConvert : public ConvertStep
{
public:

  std::map<String, std::function<SharedPtr<ConvertStep>()> > actions;

  //constructor
  DoConvert()
  {
    addAction("create", []() {return std::make_shared<CreateIdx>(); });
    addAction("server", []() {return std::make_shared<StartVisusServer>();});
    addAction("minmax", []() {return std::make_shared<FixDatasetRange>(); });
    addAction("copy-dataset", []() {return std::make_shared<CopyDataset>(); });
    addAction("compress-dataset", []() {return std::make_shared<CompressDataset>(); });
    addAction("apply-filters", []() {return std::make_shared<ApplyFilters>(); });
    addAction("convert-midx-to-idx", []() {return std::make_shared<ConvertMidxToIdx>(); });
    addAction("test-idx", []() {return std::make_shared<IdxSelfTest>(); });

    addAction("import", []() {return std::make_shared<ImportData>(); });
    addAction("export", []() {return std::make_shared<ExportData>(); });
    addAction("paste", []() {return std::make_shared<PasteData>(); });
    addAction("cast", []() {return std::make_shared<Cast>(); });
    addAction("smart-cast", []() {return std::make_shared<SmartCast>(); });
    addAction("crop", []() {return std::make_shared<CropData>(); });
    addAction("mirror", []() {return std::make_shared<MirrorData>(); });
    addAction("compute-range", []() {return std::make_shared<ComputeComponentRange>(); });
    addAction("info", []() {return std::make_shared<PrintInfo>(); });
    addAction("interleave", []() {return std::make_shared<InterleaveData>(); });
    addAction("resize", []() {return std::make_shared<ResizeData>(); });
    addAction("get-component", []() {return std::make_shared<GetComponent>(); });
    addAction("write-block", []() {return std::make_shared<ReadWriteBlock>(true); });
    addAction("read-block", []() {return std::make_shared<ReadWriteBlock>(false); });
    addAction("dump", []() {return std::make_shared<DumpData>(); });

    //cloud
    addAction("cloud-create-container", []() {return std::make_shared<CloudCreateContainer>(); });
    addAction("cloud-delete-container", []() {return std::make_shared<CloudDeleteContainer>(); });
    addAction("cloud-list-containers", []() {return std::make_shared<CloudListContainers>(); });
    addAction("cloud-list-blobs", []() {return std::make_shared<CloudDeleteBlob>(); });
    addAction("cloud-delete-blob", []() {return std::make_shared<CloudListBlobs>(); });
    addAction("cloud-copy-blob", []() {return std::make_shared<CloudCopyBlob>(); });
    addAction("cloud-self-test", []() {return std::make_shared<CloudSelfTest>(); });

    //speed
    addAction("test-query-speed", []() {return std::make_shared<TestQuerySpeed>(); });
    addAction("test-file-write-speed", []() {return std::make_shared<TestFileReadWriteSpeed>(true); });
    addAction("test-file-read-speed", []() {return std::make_shared<TestFileReadWriteSpeed>(false); });
    addAction("test-encoder-speed", []() {return std::make_shared<TestEncoderSpeed>(); });
    addAction("test-idx-slab-speed", []() {return std::make_shared<TestIdxSlabSpeed>(); });
    addAction("test-network-speed", []() {return std::make_shared<TestNetworkSpeed>(); });
  }

  //addAction
  void addAction(String name, std::function< SharedPtr<ConvertStep>()> fn)
  {
    actions[name] = fn;
  }

  //getHelp
  virtual String getHelp() override
  {
    std::ostringstream out;
    out << "Syntax: " << std::endl << ApplicationInfo::args[0] << std::endl;
    for (auto it : actions)
      out << "    " << it.first << std::endl;
    out << std::endl;
    out << "For specific help: " << ApplicationInfo::args[0] << " <action-name> help";
    out << std::endl;
    return out.str();
  }

  //exec
  virtual Array exec(Array data, std::vector<String> args) override
  {
    if (args.size() == 1 || (args.size() == 2 && (args[1] == "help" || args[1] == "--help" || args[1] == "-h")))
    {
      std::cout<<getHelp();
      return Array();
    }

    std::vector< std::vector<String> > parsed;

    for (int I=1;I<(int)args.size();I++)
    {
      String arg=args[I];

      String name = StringUtils::toLower(arg);
      while (name[0] == '-')
        name = name.substr(1);

      //begin of a new command
      if (actions.find(name) != actions.end())
      {
        parsed.push_back(std::vector<String>({ name }));
        continue;
      }

      if (parsed.empty())
      {
        std::ostringstream error_msg;
        error_msg << "Wrong argument " << arg << std::endl;
        error_msg << getHelp();
        ThrowException(error_msg.str());
      }

      parsed.back().push_back(arg);
    }

    for (auto args : parsed)
    {
      String name = args[0];

      VisusInfo() << "// *** STEP "<< name<<" ***";
      VisusInfo() << "Input dtype(" << data.dtype.toString() << ") " << "dims(" << data.dims.toString() << ") args("<<StringUtils::join(args," ")<<")";

      auto action = actions[name]();

      if (args.size() == 2 && (args[1] == "help" || args[1] == "--help" || args[1] == "-h"))
      {
        VisusInfo() << std::endl << args[0] << " " << name << " " << action->getHelp();
        return data;
      }

      Time t1 = Time::now();
      data = action->exec(data, args);
      VisusInfo() << "STEP "<<name<<" done in " << t1.elapsedMsec() << "msec";
    }

    return data;
  }
};


//////////////////////////////////////////////////////////////////////////////
int main(int argn, const char* argv[])
{
  Time T1 = Time::now();

  SetCommandLine(argn, argv);
  IdxModule::attach();

  Array data;
  DoConvert convert;

  if (ApplicationInfo::debug)
  {
    data = convert.exec(data, ApplicationInfo::args);
  }
  else
  {
    try
    {
      data = convert.exec(data, ApplicationInfo::args);
    }
    catch (Exception& ex)
    {
      VisusInfo() << "ERROR: " << ex.what();
      IdxModule::detach();
      return -1;
    }
  }

  VisusInfo() << "All done in " << T1.elapsedSec()<< " seconds";

  IdxModule::detach();

  return 0;
}
