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

#ifndef VISUS_GOOGLE_CLOUD_STORAGE_
#define VISUS_GOOGLE_CLOUD_STORAGE_

#include <Visus/Kernel.h>
#include <Visus/CloudStorage.h>

#define JSON_SKIP_UNSUPPORTED_COMPILER_CHECK 1
#include <Visus/json.hpp>

namespace Visus {

////////////////////////////////////////////////////////////////////////////////
class GoogleDriveStorage : public CloudStorage
{
public:

  /*

  # https://console.developers.google.com/apis/credentials
  # create a project "visus"
  # crea credential ID Client Oath with "Other" and name "visus-client"
  # take note of client_id and client_secret

  client_id="AAAAAAA"
  client_secret="BBBBBB"

  # need to activate a device using a browser
  # echo "https://accounts.google.com/o/oauth2/auth?client_id=$client_id&response_type=code&scope=https://www.googleapis.com/auth/drive&redirect_uri=urn:ietf:wg:oauth:2.0:oob"
  # copy the code here
  code="CCCCCCCCCCC"

  # get access_token and refresh_token
  curl \
  --data "code=$code" \
  --data "client_id=$client_id" \
  --data "client_secret=$client_secret" \
  --data "redirect_uri=urn:ietf:wg:oauth:2.0:oob" \
  --data "grant_type=authorization_code" \
  "https://www.googleapis.com/oauth2/v3/token"

  access_token="DDDDDDDD"
  refresh_token="EEEEEEEEE"

  */

  VISUS_CLASS(GoogleDriveStorage)

    Url    url;
  String client_id;
  String client_secret;
  String refresh_token;

  struct
  {
    String value;
    Time   t1;
    double expires_in = 0;
  }
  access_token;

  std::map<String, String> container_ids;

  //constructor
  GoogleDriveStorage(Url url)
  {
    this->refresh_token = url.getParam("refresh_token"); VisusAssert(!refresh_token.empty());
    this->client_id = url.getParam("client_id"); VisusAssert(!client_id.empty());
    this->client_secret = url.getParam("client_secret"); VisusAssert(!client_secret.empty());

    this->url = url.getProtocol() + "://" + url.getHostname();

    this->container_ids[""] = "root";
  }

  //destructor
  virtual ~GoogleDriveStorage() {
  }

  //signRequest
  void signRequest(NetRequest& request)
  {
    //need to regenerate access token?
    if (this->access_token.value.empty() || this->access_token.t1.elapsedSec() > 0.85* this->access_token.expires_in)
    {
      this->access_token.value = "";

      NetRequest request(Url(this->url.toString() + "/oauth2/v3/token"), "POST");
      request.setTextBody(concatenate(
        "client_id=", this->client_id,
        "&", "client_secret=", this->client_secret,
        "&", "refresh_token=", this->refresh_token,
        "&", "grant_type=refresh_token"));

      auto response = NetService::getNetResponse(request);
      if (response.isSuccessful())
      {
        auto json = nlohmann::json::parse(response.getTextBody());
        this->access_token.t1 = Time::now();
        this->access_token.value = json["access_token"].get<std::string>();
        this->access_token.expires_in = json["expires_in"].get<int>();
      }
    }

    request.headers.setValue("Authorization", "Bearer " + this->access_token.value);
  }

  // recursiveGetContainerId
  void recursiveGetContainerId(Future<String> ret, SharedPtr<NetService> service, String current, String last, bool bCreate, Aborted aborted = Aborted())
  {
    VisusAssert(StringUtils::startsWith(last, current));
    VisusAssert(container_ids.find(current) != container_ids.end());

    //final part of the recursion
    if (current == last)
      return ret.get_promise()->set_value(container_ids[current]);

    //take only the first part that needs to be created
    auto name = StringUtils::split(last.substr(current.size()), "/")[0];

    auto next = current + "/" + name;

    //already know
    if (container_ids.find(next) != container_ids.end())
      return recursiveGetContainerId(ret, service, next, last, bCreate, aborted);

    //check if exists (don't want to create duplicates)
    std::ostringstream url;
    url << this->url.toString() << "/drive/v3/files?q=name='" << name << "'";
    url << " and '" << container_ids[current] << "' in parents";

    NetRequest request(url.str(), "GET");
    request.aborted = aborted;
    signRequest(request);
    NetService::push(service, request).when_ready([this, service, ret, current, next, last, name, bCreate, aborted](NetResponse response) {

      if (!response.isSuccessful())
        return ret.get_promise()->set_value("");

      //already exists? 
      auto json = nlohmann::json::parse(response.getTextBody());
      auto id = json["files"].size() ? json["files"].at(0)["id"].get<std::string>() : String();
      if (id.size())
      {
        container_ids[next] = id;
        return recursiveGetContainerId(ret, service, next, last, bCreate, aborted);
      }

      if (!bCreate)
        return ret.get_promise()->set_value("");

      //need to create one
      std::ostringstream body;
      body << "{";
      body << "'name':'" + name + "'";
      body << " ,'mimeType':'application/vnd.google-apps.folder'";
      body << " ,'parents':['" + container_ids[current] + "']";
      body << "}";

      NetRequest request(Url(this->url.toString() + "/drive/v3/files"), "POST");
      request.aborted = aborted;
      request.setHeader("Content-Type", "application/json");
      request.setTextBody(body.str());
      signRequest(request);

      NetService::push(service, request).when_ready([this, ret, service, current, next, last, bCreate, aborted](NetResponse response) {

        if (!response.isSuccessful())
          return ret.get_promise()->set_value("");

        auto json = nlohmann::json::parse(response.getTextBody());
        container_ids[next] = json["id"].get<std::string>();
        return recursiveGetContainerId(ret, service, next, last, bCreate, aborted);
      });
    });
  }

  //getContainerId
  Future<String> getContainerId(SharedPtr<NetService> service, String container_name, bool bCreate, Aborted aborted = Aborted())
  {
    auto ret = Promise<String>().get_future();
    recursiveGetContainerId(ret, service, "", container_name, bCreate, aborted);
    return ret;
  }

  // addContainer (if already exists do not create)
  Future<String> addContainer(SharedPtr<NetService> service, String container_name, Aborted aborted = Aborted())
  {
    return getContainerId(service, container_name,/*bCreate*/true, aborted);
  }

  // deleteContainer
  Future<bool> deleteContainer(SharedPtr<NetService> service, String container_name, Aborted aborted = Aborted())
  {
    auto ret = Promise<bool>().get_future();

    getContainerId(service, container_name,/*bCreate*/false, aborted).when_ready([this, service, ret](String container_id) {

      if (container_id.empty())
        return ret.get_promise()->set_value(false);

      NetRequest request(Url(this->url.toString() + "/drive/v3/files/" + container_id), "DELETE");
      NetService::push(service, request).when_ready([ret](NetResponse response) {
        ret.get_promise()->set_value(response.isSuccessful());
      });

    });

    return ret;
  }


  // addBlobRequest 
  virtual Future<bool> addBlob(SharedPtr<NetService> service, String blob_name, Blob blob, Aborted aborted = Aborted()) override
  {
    auto ret = Promise<bool>().get_future();

    auto index = blob_name.rfind('/');
    if (index == String::npos)
    {
      VisusAssert(false);
      ret.get_promise()->set_value("");
      return ret;
    }

    auto container_name = blob_name.substr(0, index);

    getContainerId(service, container_name,/*bCreate*/true, aborted).when_ready([this, service, ret, blob_name, blob, aborted](String container_id) {

      if (container_id.empty())
      {
        ret.get_promise()->set_value(false);
        return;
      }

      String name = StringUtils::split(blob_name, "/").back();

      NetRequest request(Url(this->url.toString() + "/upload/drive/v3/files?uploadType=multipart"), "POST");
      request.aborted = aborted;

      String boundary = UUIDGenerator::getSingleton()->create();
      request.setHeader("Expect", "");
      request.setContentType("multipart/form-data; boundary=" + boundary);

      //write multiparts
      {
        request.body = std::make_shared<HeapMemory>();

        OutputBinaryStream out(*request.body);

        //multipart 1
        out << "--" << boundary << "\r\n";
        out << "Content-Disposition: form-data; name=\"metadata\"\r\n";
        out << "Content-Type: application/json;charset=UTF-8;\r\n";
        out << "\r\n";
        out << "{'name':'" << name << "','parents':['" << container_id << "'], 'properties': {";
        bool need_sep = false;
        for (auto it : blob.metadata)
        {
          out << (need_sep ? "," : "") << "'" << it.first << "': '" << it.second << "'";
          need_sep = true;
        }
        out << "}}" << "\r\n";

        //multipart2
        out << "--" << boundary << "\r\n";
        out << "Content-Disposition: form-data; name=\"file\"\r\n";
        out << "Content-Type: " + blob.content_type + "\r\n";
        out << "\r\n";
        out << *blob.body << "\r\n";

        //multipart end
        out << "--" << boundary << "--";

        request.setContentLength(request.body->c_size());
      }

      signRequest(request);

      NetService::push(service, request).when_ready([ret](NetResponse response) {
        ret.get_promise()->set_value(response.isSuccessful());
      });


    });

    return ret;
  }

  // getBlob 
  virtual Future<Blob> getBlob(SharedPtr<NetService> service, String blob_name, Aborted aborted = Aborted()) override
  {
    auto ret = Promise<Blob>().get_future();

    auto index = blob_name.rfind('/');
    if (index == String::npos)
    {
      VisusAssert(false);
      ret.get_promise()->set_value(Blob());
      return ret;
    }
    auto container_name = blob_name.substr(0, index);


    getContainerId(service, container_name,/*bCreate*/false, aborted).when_ready([this, ret, service, blob_name, aborted](String container_id) {

      if (container_id.empty())
      {
        ret.get_promise()->set_value(Blob());
        return;
      }

      String name = StringUtils::split(blob_name, "/").back();

      NetRequest get_blob_id(this->url.toString() + "/drive/v3/files?q=name='" + name + "' and '" + container_id + "' in parents", "GET");
      get_blob_id.aborted = aborted;
      signRequest(get_blob_id);

      NetService::push(service, get_blob_id).when_ready([this, service, ret, aborted](NetResponse response) {

        if (!response.isSuccessful())
        {
          VisusWarning() << "ERROR. Cannot get blob status(" << response.status << "),errormsg(" << response.getErrorMessage() << ")";
          ret.get_promise()->set_value(Blob());
          return;
        }

        auto json = nlohmann::json::parse(response.getTextBody());
        auto blob_id = json["files"].size() ? json["files"].at(0)["id"].get<std::string>() : String();
        if (blob_id.empty())
        {
          ret.get_promise()->set_value(Blob());
          return;
        }

        NetRequest get_blob_metadata(Url(this->url.toString() + "/drive/v3/files/" + blob_id + "?fields=id,name,mimeType,properties"), "GET");
        get_blob_metadata.aborted = aborted;
        signRequest(get_blob_metadata);

        NetService::push(service, get_blob_metadata).when_ready([this, ret, service, blob_id, aborted](NetResponse response) {

          if (!response.isSuccessful())
          {
            VisusWarning() << "ERROR. Cannot get blob status(" << response.status << "),errormsg(" << response.getErrorMessage() << ")";
            ret.get_promise()->set_value(Blob());
            return;
          }

          auto metadata = StringMap();
          auto json = nlohmann::json::parse(response.getTextBody());
          for (auto it : json["properties"].get<nlohmann::json::object_t>())
          {
            auto key = it.first;
            auto value = it.second.get<String>();
            metadata.setValue(key, value);
          }

          NetRequest get_blob_media(Url(this->url.toString() + "/drive/v3/files/" + blob_id + "?alt=media"), "GET");
          get_blob_media.aborted = aborted;
          signRequest(get_blob_media);

          NetService::push(service, get_blob_media).when_ready([ret, aborted, metadata](NetResponse response) {

            if (!response.isSuccessful())
            {
              VisusWarning() << "ERROR. Cannot get blob status(" << response.status << "),errormsg(" << response.getErrorMessage() << ")";
              ret.get_promise()->set_value(Blob());
              return;
            }

            auto content_type = response.getContentType();

            Blob blob;
            blob.metadata = metadata;
            blob.body = response.body;
            if (!content_type.empty())
              blob.content_type = content_type;

            ret.get_promise()->set_value(blob);
          });
        });

      });
    });

    return ret;
  }

  // deleteBlob
  virtual Future<bool> deleteBlob(SharedPtr<NetService> service, String blob_name, Aborted aborted = Aborted()) override
  {
    auto ret = Promise<bool>().get_future();

    auto index = blob_name.rfind('/');
    if (index == String::npos)
    {
      VisusAssert(false);
      ret.get_promise()->set_value(false);
      return ret;
    }
    auto container_name = blob_name.substr(0, index);

    getContainerId(service, container_name,/*bCreate*/false, aborted).when_ready([this, ret, service, blob_name, aborted](String container_id) {

      if (container_id.empty())
        return ret.get_promise()->set_value(false);

      String name = StringUtils::split(blob_name, "/").back();

      NetRequest get_blob_id(this->url.toString() + "/drive/v3/files?q=name='" + name + "' and '" + container_id + "' in parents", "GET");
      get_blob_id.aborted = aborted;
      signRequest(get_blob_id);

      NetService::push(service, get_blob_id).when_ready([this, service, ret, aborted](NetResponse response) {

        if (!response.isSuccessful())
        {
          ret.get_promise()->set_value(false);
          return;
        }

        auto json = nlohmann::json::parse(response.getTextBody());
        auto blob_id = json["files"].size() ? json["files"].at(0)["id"].get<std::string>() : String();
        if (blob_id.empty())
        {
          ret.get_promise()->set_value(false);
          return;
        }

        NetRequest delete_blob(Url(this->url.toString() + "/drive/v3/files/" + blob_id), "DELETE");
        delete_blob.aborted = aborted;
        signRequest(delete_blob);

        NetService::push(service, delete_blob).when_ready([ret](NetResponse response) {
          ret.get_promise()->set_value(response.isSuccessful());
        });

      });
    });

    return ret;
  }

};


}//namespace

#endif //VISUS_GOOGLE_CLOUD_STORAGE_

