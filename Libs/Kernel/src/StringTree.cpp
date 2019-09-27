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

#include <Visus/StringTree.h>
#include <Visus/StringUtils.h>
#include <Visus/Log.h>
#include <Visus/Path.h>
#include <Visus/Utils.h>
#include <Visus/ApplicationInfo.h>

#include <tinyxml/tinyxml.h>

#include <algorithm>
#include <iomanip>



namespace Visus {

//////////////////////////////////////////////////////////////////////////////////////////////////
class PostProcessStringTree
{
private:

  //acceptAlias
  /* Example:
    <alias key='server' value='atlantis.sci.utah.edu'/>
  */
  static StringMap acceptAlias(StringTree& dst,StringTree& src,std::map<String,StringTree*> templates,StringMap aliases)
  {
    String key  =src.readString("key");
    String value=resolveAliases(src.readString("value"),aliases);
    VisusAssert(!key.empty() && !value.empty());
    aliases.setValue(key,value);

    //an alias can have childs too!
    for (auto src_child : src.childs)  
    {
      auto dst_child = std::make_shared<StringTree>();
      dst.addChild(dst_child);
      accept(*dst_child,*src_child,templates,aliases);
    }

    return aliases;
  }

  //resolveAliases
  /* Example:
    <dataset url='$(server)' />
  */
  static String resolveAliases(String value,const StringMap& aliases)
  {
    if (!StringUtils::contains(value,"$"))
      return value;

    for (auto it=aliases.begin();it!=aliases.end();it++)
      value=StringUtils::replaceAll(value,"$(" + it->first + ")",it->second);

    //VisusAssert(!StringUtils::contains(value,"$")); 
    return value;
  }

  //acceptDefineTemplate
  /* Example:
  
      <template name='myname'>
        ...
      </template>
  */
  static std::map<String,StringTree*> acceptDefineTemplate(StringTree& dst,StringTree& src,std::map<String,StringTree*> templates,StringMap aliases)
  {
    String template_name=src.readString("name"); 
    VisusAssert(!template_name.empty());
    templates[template_name]=&src;
    return templates;
  }

  //acceptCallTemplate
  /* Example:
    <call template="myname" />
  */
  static void acceptCallTemplate(StringTree& dst,StringTree& src,std::map<String,StringTree*> templates,StringMap aliases)
  {
    VisusAssert(src.getNumberOfChilds()==0); //not supported child of calls
    String template_name=src.readString("template");
    VisusAssert(!template_name.empty());
    VisusAssert(templates.find(template_name)!=templates.end());

    //expand the template corrent into dst
    for (int I=0;I<(int)templates[template_name]->getNumberOfChilds();I++) 
    {
      auto child = std::make_shared<StringTree>();
      dst.addChild(child);
      accept(*child,*templates[template_name]->childs[I],templates,aliases);
    }
  }

  //acceptInclude
  /* Example:
    <include url="..." />
  */
  static void acceptInclude(StringTree& dst,StringTree& src,std::map<String,StringTree*> templates,StringMap aliases)
  {
    VisusAssert(src.getNumberOfChilds()==0); //<include> should not have any childs!
    String url=src.readString("url");
    VisusAssert(!url.empty());

    StringTree inplace;
    if (!inplace.fromXmlString(Utils::loadTextDocument(url)))
    {
      VisusInfo()<<"cannot load document "<<url;
      VisusAssert(false);
      return;
    }

    //'explode' all childs inside the current context
    if (inplace.name == "visus")
    {
      VisusAssert(inplace.attributes.empty());

      for (auto child : inplace.childs)
      {
        auto dst_child = std::make_shared<StringTree>();
        dst.addChild(dst_child);
        accept(*dst_child, *child, templates, aliases);
      }
    }
    else
    {
      accept(dst, inplace, templates, aliases);
    }
  }

  //acceptIf
  /* Example: 
    
  <if condition=''>
    <then>
      ...
    </then>
    ....
    <else>
      ...
    </else>
  </if>
  */
  static void acceptIf(StringTree& dst, StringTree& src, std::map<String, StringTree*> templates, StringMap aliases)
  {
    String condition_expr = src.readString("condition"); VisusAssert(!condition_expr.empty());
    
    //TODO: mini parser here?
    bool bCondition = false;
    if      (condition_expr ==  "win"  ) bCondition = ApplicationInfo::platform_name == "win";
    else if (condition_expr == "!win"  ) bCondition = ApplicationInfo::platform_name != "win";
    else if (condition_expr ==  "osx"  ) bCondition = ApplicationInfo::platform_name == "osx";
    else if (condition_expr == "!osx"  ) bCondition = ApplicationInfo::platform_name != "osx";
    else if (condition_expr ==  "ios"  ) bCondition = ApplicationInfo::platform_name == "ios";
    else if (condition_expr == "!ios"  ) bCondition = ApplicationInfo::platform_name != "ios";
    else if (condition_expr ==  "linux") bCondition = ApplicationInfo::platform_name == "linux";
    else if (condition_expr == "!linux") bCondition = ApplicationInfo::platform_name != "linux";
    else bCondition = cbool(condition_expr);

    for (auto child : src.childs)
    {
      if (child->name == "then")
      {
        if (bCondition)
        {
          for (auto then_child : child->childs)
          {
            StringTree tmp;
            accept(tmp, *then_child, templates, aliases);
            dst.addChild(tmp);
          }
        }
      }
      else if (child->name == "else")
      {
        if (!bCondition)
        {
          for (auto else_child : child->childs)
          {
            StringTree tmp;
            accept(tmp, *else_child, templates, aliases);
            dst.addChild(tmp);
          }
        }
        continue;
      }
      else
      {
        VisusAssert(false);
      }


    }
  }

  //accept
  static void accept(StringTree& dst,StringTree& src,std::map<String,StringTree*> templates,StringMap aliases)
  {
    if (src.name=="alias")
    {
      aliases=acceptAlias(dst,src,templates,aliases);
      return;
    }

    if (src.name=="template")
    {
      templates=acceptDefineTemplate(dst,src,templates,aliases);
      return;
    }

    if (src.name=="call")
    {
      acceptCallTemplate(dst,src,templates,aliases);
      return;
    }

    if (src.name=="include")
    {
      acceptInclude(dst,src,templates,aliases);
      return;
    }

    if (src.name == "if")
    {
      acceptIf(dst, src, templates, aliases);
      return;
    }

    //recursive
    {
      //name
      dst.name=src.name;

      //attributes
      for (auto it : src.attributes)
      {
        String key   = it.first;
        String value = resolveAliases(it.second,aliases);
        dst.writeString(key,value);
      }

      //childs
      for (auto child : src.childs) 
      {
        if (child->name=="alias")
        {
          aliases=acceptAlias(dst,*child,templates,aliases);
          continue;
        }

        if (child->name=="template")
        {
          templates=acceptDefineTemplate(dst, *child,templates,aliases);
          continue;
        }

        if (child->name=="call")
        {
          acceptCallTemplate(dst, *child,templates,aliases);
          continue;
        }

        if (child->name=="include")
        {
          acceptInclude(dst, *child,templates,aliases);
          continue;
        }

        if (child->name == "if")
        {
          acceptIf(dst, *child, templates, aliases);
          continue;
        }

        dst.addChild(StringTree());
        accept(*dst.childs.back(), *child,templates,aliases);
      }
    }

  }

public:

  //exec
  static StringTree exec(StringTree& src)
  {
    std::map<String,StringTree*> templates;
    StringMap      aliases;
    aliases.setValue("VisusHome"              , KnownPaths::VisusHome                .toString());
    aliases.setValue("BinaryDirectory"        , KnownPaths::BinaryDirectory          .toString());
    aliases.setValue("CurrentWorkingDirectory", KnownPaths::CurrentWorkingDirectory().toString());

    StringTree dst;
    accept(dst,src,templates,aliases);
    return dst;
  }

};

/////////////////////////////////////////////////
StringTree StringTree::postProcess(const StringTree& src)
{
  auto dst=PostProcessStringTree::exec(const_cast<StringTree&>(src));
  //VisusInfo() << dst.toString();
  return dst;
}

/////////////////////////////////////////////////
String StringTree::readText() const
{
  std::ostringstream out;
  for (int I=0;I<childs.size();I++)
  {
    if (childs[I]->name=="#text")         
      out<<childs[I]->readString("value");
    
    else if (childs[I]->name== "#cdata-section") 
      out<<childs[I]->readString("value");
  }
  return out.str();
}


/////////////////////////////////////////////////
String StringTree::readString(String key,String default_value) const
{
  if (StringUtils::contains(key, "/"))
  {
    std::vector<String> vkey = StringUtils::split(key, "/");
    if (vkey.empty())
    {
      VisusAssert(false);
      return default_value;
    }
    StringTree* cursor = const_cast<StringTree*>(this);
    for (int I = 0; cursor && I < (int)vkey.size() - 1; I++)
      cursor = cursor->findChildWithName(vkey[I]);

    return cursor ? cursor->readString(vkey.back(), default_value) : default_value;
  }
  else
  {
    VisusAssert(!key.empty());
    return getAttribute(key,default_value);
  }
}

/////////////////////////////////////////////////
void StringTree::writeString(String key,String value)
{
  if (StringUtils::contains(key, "/"))
  {
    std::vector<String> vkey = StringUtils::split(key, "/");
    if (vkey.empty())
    {
      VisusAssert(false); return;
    }
    StringTree* cursor = const_cast<StringTree*>(this);
    for (int I = 0; I < (int)vkey.size() - 1; I++)
    {
      if (!cursor->findChildWithName(vkey[I]))
        cursor->addChild(StringTree(vkey[I]));
      cursor = cursor->findChildWithName(vkey[I]);
    }
    cursor->writeString(vkey.back(), value);
  }
  else
  {
    VisusAssert(!key.empty());
    setAttribute(key,value);
  }
}

/////////////////////////////////////////////////
StringTree* StringTree::findChildWithName(String name,StringTree* prev) const
{
  if (StringUtils::contains(name, "/"))
  {
    std::vector<String> vname = StringUtils::split(name, "/");
    if (vname.empty())
    {
      VisusAssert(false);
      return nullptr;
    }
    StringTree* cursor = const_cast<StringTree*>(this);
    for (int I = 0; cursor && I < (int)vname.size() - 1; I++)
      cursor = cursor->findChildWithName(vname[I]);

    return cursor ? cursor->findChildWithName(vname.back(), prev) : nullptr;
  }
  else
  {
    int I=0;

    if (prev)
    {
      for (;I<(int)childs.size();I++)
      {
        if (childs[I].get()==prev) 
        {
          I++;
          break;
        }
      }
    }

    for (;I<(int)childs.size();I++)
    {
      if (childs[I]->name==name) 
        return const_cast<StringTree*>(childs[I].get());
    }

    return NULL; //not found
  }
}

/////////////////////////////////////////////////
std::vector<StringTree*> StringTree::findAllChildsWithName(String name,bool bRecursive) const
{
  std::vector<StringTree*> ret;
  for (int I=0;I<(int)childs.size();I++)
  {
    if (name.empty() || childs[I]->name==name) ret.push_back(const_cast<StringTree*>(childs[I].get()));
    if (bRecursive)
    {
      std::vector<StringTree*> sub=childs[I]->findAllChildsWithName(name,true);
      ret.insert(ret.end(),sub.begin(),sub.end());
    }
  }
  return ret;
}

/////////////////////////////////////////////////
int StringTree::getMaxDepth()
{
  int ret=0;
  for (auto child : this->childs)
    ret=std::max(ret,1+ child->getMaxDepth());
  return ret;
}

/////////////////////////////////////////////////
void StringTree::writeToObjectStream(ObjectStream& out)
{
  StringTree* dst=out.getCurrentContext();
  StringTree* src=this;
  VisusAssert(dst->empty() && (dst->name.empty() || dst->name==src->name));
  *dst=*src;
}

/////////////////////////////////////////////////
void StringTree::readFromObjectStream(ObjectStream& in)
{
  StringTree* dst=this;
  StringTree* src=in.getCurrentContext();
  VisusAssert(dst->empty() && (dst->name.empty() || dst->name==src->name));
  *dst=*src;
}


static TiXmlElement* ToXmlElement(const StringTree& src)
{
  TiXmlElement* dst = new TiXmlElement(src.name.c_str());

  for (auto it = src.attributes.begin(); it != src.attributes.end(); it++)
  {
    String key = it->first;
    String value = it->second;
    dst->SetAttribute(key.c_str(), value.c_str());
  }

  for (auto child : src.childs)
  {
    if (child->name== "#cdata-section")
    {
      VisusAssert(child->attributes.size() == 1 && child->hasAttribute("value") && child->getNumberOfChilds() == 0);
      String text = child->readString("value");
      TiXmlText * ti_xml_text = new TiXmlText(text.c_str());
      ti_xml_text->SetCDATA(true);
      dst->LinkEndChild(ti_xml_text);
    }
    else if (child->name=="#text")
    {
      VisusAssert(child->attributes.size() == 1 && child->hasAttribute("value") && child->getNumberOfChilds() == 0);
      String text = child->readString("value");
      TiXmlText * ti_xml_text = new TiXmlText(text.c_str());
      ti_xml_text->SetCDATA(false);
      dst->LinkEndChild(ti_xml_text);
    }
    else
    {
      dst->LinkEndChild(ToXmlElement(*child));
    }
  }

  return dst;
}

///////////////////////////////////////////////////////////////////////////
String StringTree::toXmlString() const
{
  TiXmlDocument* xmldoc = new TiXmlDocument();
  xmldoc->LinkEndChild(new TiXmlDeclaration("1.0", "", ""));
  xmldoc->LinkEndChild(ToXmlElement(*this));

  TiXmlPrinter printer;
  printer.SetIndent("\t");
  printer.SetLineBreak("\n");
  xmldoc->Accept(&printer);
  String ret(printer.CStr());
  delete xmldoc;

  ret = StringUtils::replaceAll(ret, "<?xml version=\"1.0\" ?>", "");
  ret = StringUtils::trim(ret);
  return ret;
}

///////////////////////////////////////////////////////////////////////////
static String FormatJSON(String str)
{
  String nStr = str;
  for (size_t i = 0; i < nStr.size(); i++)
  {
    String sreplace = "";
    bool replace = true;

    switch (nStr[i])
    {
      case '\\': sreplace = "\\\\";    break;
      case '\n': sreplace = "\\n\\\n"; break;
      case '\r': sreplace = "\\r";     break;
      case '\a': sreplace = "\\a";     break;
      case '"':  sreplace = "\\\"";    break;
      default:
      {
        int nCh = ((int)nStr[i]) & 0xFF;
        if (nCh < 32 || nCh>127)
        {
          char buffer[5];
  #if VISUS_WIN
          sprintf_s(buffer, 5, "\\x%02X", nCh);
  #else
          snprintf(buffer, 5, "\\x%02X", nCh);
  #endif
          sreplace = buffer;
        }
        else
          replace = false;
      }
    }

    if (replace)
    {
      nStr = nStr.substr(0, i) + sreplace + nStr.substr(i + 1);
      i += sreplace.length() - 1;
    }
  }
  return "\"" + nStr + "\"";
};


/////////////////////////////////////////////////
String StringTree::toJSONString(const StringTree& src, int nrec) 
{
  std::ostringstream out;

  out << "{" << std::endl;
  {
    out << FormatJSON("name")      << " : " << FormatJSON(src.name) << "," << std::endl;
    out << FormatJSON("attributes") << " : { " << std::endl;
    {
      int I = 0, N = (int)src.attributes.size(); 
      for (const auto& it : src.attributes) {
        out << FormatJSON(it.first) << " : " << FormatJSON(it.second) << ((I != N - 1) ? "," : "") << std::endl;
        I++;
      }
    }
    out << "}," << std::endl;
    out << FormatJSON("childs") << " : [ " << std::endl;
    {
      int I = 0, N = (int)src.childs.size(); 
      for (const auto& child : src.childs)
      {
        out << toJSONString(*child, nrec + 1) << ((I != N - 1) ? "," : "") << std::endl;
        I++;
      }
    }
    out << "]" << std::endl;
  }
  out << "}" << std::endl;

  return out.str();
}

////////////////////////////////////////////////////////////////////////
static StringTree FromXmlElement(TiXmlElement* src)
{
  StringTree dst(src->Value());

  for (TiXmlAttribute* attr = src->FirstAttribute(); attr; attr = attr->Next())
    dst.writeString(attr->Name(), attr->Value());

  //xml_text                                              
  if (const TiXmlNode* child = src->FirstChild())
  {
    if (const TiXmlText* child_text = child->ToText())
    {
      if (const char* xml_text = child_text->Value())
      {
        dst.writeText(xml_text, child_text->CDATA());
      }
    }
    else if (const TiXmlComment* child_comment = child->ToComment())
    {
      dst.addCommentNode(child_comment->Value());
    }
  }

  for (TiXmlElement* child = src->FirstChildElement(); child; child = child->NextSiblingElement())
    dst.addChild(FromXmlElement(child));

  return dst;
}




////////////////////////////////////////////////////////////
bool StringTree::fromXmlString(String content, bool bEnablePostProcessing)
{
  if (content.empty())
  {
    VisusWarning() << "StringTree::fromXmlString failed because of empty content";
    return false;
  }

  TiXmlDocument xmldoc;
  xmldoc.Parse(content.c_str());
  if (xmldoc.Error())
  {
    VisusWarning() << "Failed StringTree::fromXmlString failed"
      << " ErrorRow(" << xmldoc.ErrorRow() << ")"
      << " ErrorCol(" << xmldoc.ErrorCol() << ")"
      << " ErrorDesc(" << xmldoc.ErrorDesc() << ")";
    return false;
  }

  (*this) = FromXmlElement(xmldoc.FirstChildElement());

  if (bEnablePostProcessing)
    *this = StringTree::postProcess(*this);

  return true;
}


VISUS_IMPLEMENT_SINGLETON_CLASS(Private::VisusConfig)

//////////////////////////////////////////////////////////////
bool ConfigFile::load(String filename, bool bEnablePostProcessing)
{
  if (filename.empty()) {
    VisusAssert(false);
    return false;
  }

  String body = Utils::loadTextDocument(filename);

  StringTree temp(this->name);
  if (!temp.fromXmlString(body, bEnablePostProcessing))
  {
    VisusWarning() << "visus config content is wrong or empty";
    return false;
  }

  this->filename = filename;
  this->StringTree::operator=(temp);

  return true;
}

//////////////////////////////////////////////////////////////
bool ConfigFile::save()
{
  if (this->filename.empty())
    return false;

  if (!Utils::saveTextDocument(filename, this->StringTree::toXmlString()))
    return false;

  return true;
}

} //namespace Visus






