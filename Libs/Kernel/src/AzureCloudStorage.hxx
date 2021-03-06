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

#ifndef VISUS_AZURE_CLOUD_STORAGE_
#define VISUS_AZURE_CLOUD_STORAGE_

#include <Visus/Kernel.h>
#include <Visus/CloudStorage.h>

namespace Visus {

//////////////////////////////////////////////////////////////////////////////// 
class AzureCloudStorage : public CloudStorage
{
public:

  //constructor
  AzureCloudStorage(Url url)
  {
    this->access_key = url.getParam("access_key");
    VisusAssert(!access_key.empty());
    this->access_key = StringUtils::base64Decode(access_key);

    this->account_name = StringUtils::split(url.getHostname(),".")[0];

    this->url = url.getProtocol() + "://" + url.getHostname();
  }

  //destructor
  virtual ~AzureCloudStorage() {
  }

  // getBlob 
  virtual Future<CloudStorageBlob> getBlob(SharedPtr<NetService> service, String blob_name, Aborted aborted = Aborted()) override
  {
    auto ret = Promise<CloudStorageBlob>().get_future();

    NetRequest request(this->url.toString() + blob_name, "GET");
    request.aborted = aborted;

    if (!access_key.empty())
      signRequest(request);

    NetService::push(service, request).when_ready([ret](NetResponse response) {

      if (!response.isSuccessful())
      {
        ret.get_promise()->set_value(CloudStorageBlob());
        return;
      }

      //parse metadata
      CloudStorageBlob blob;
      String metatata_prefix = "x-ms-meta-";
      for (auto it = response.headers.begin(); it != response.headers.end(); it++)
      {
        String name = it->first;
        if (StringUtils::startsWith(name, metatata_prefix))
        {
          name = name.substr(metatata_prefix.length());

          //trick: azure does not allow the "-" 
          if (StringUtils::contains(name, "_"))
            name = StringUtils::replaceAll(name, "_", "-");

          blob.metadata.setValue(name, it->second);
        }
      }

      blob.body = response.body;

      auto content_type = response.getContentType();
      if (!content_type.empty())
        blob.content_type = content_type;

      ret.get_promise()->set_value(blob);
    });

    return ret;
  }

private:

  Url    url;
  String account_name;
  String access_key;

  String container;

  //signRequest
  void signRequest(NetRequest& request)
  {
    String canonicalized_resource = "/" + this->account_name + request.url.getPath();

    if (!request.url.params.empty())
    {
      std::ostringstream out;
      for (auto it = request.url.params.begin(); it != request.url.params.end(); ++it)
        out << "\n" << it->first << ":" << it->second;
      canonicalized_resource += out.str();
    }

    char date_GTM[256];
    time_t t; time(&t);
    struct tm* ptm = gmtime(&t);
    strftime(date_GTM, sizeof(date_GTM), "%a, %d %b %Y %H:%M:%S GMT", ptm);

    request.setHeader("x-ms-version", "2018-03-28");
    request.setHeader("x-ms-date", date_GTM);

    String canonicalized_headers;
    {
      std::ostringstream out;
      int N = 0; for (auto it = request.headers.begin(); it != request.headers.end(); it++)
      {
        if (StringUtils::startsWith(it->first, "x-ms-"))
          out << (N++ ? "\n" : "") << StringUtils::toLower(it->first) << ":" << it->second;
      }
      canonicalized_headers = out.str();
    }

    /*
    In the current version, the Content-Length field must be an empty string if the content length of the request is zero.
    In version 2014-02-14 and earlier, the content length was included even if zero.
    See below for more information on the old behavior
    */
    String content_length = request.getHeader("Content-Length");
    if (cint(content_length) == 0)
      content_length = "";

    String signature;
    signature += request.method + "\n";// Verb
    signature += request.getHeader("Content-Encoding") + "\n";
    signature += request.getHeader("Content-Language") + "\n";
    signature += content_length + "\n";
    signature += request.getHeader("Content-MD5") + "\n";
    signature += request.getHeader("Content-Type") + "\n";
    signature += request.getHeader("Date") + "\n";
    signature += request.getHeader("If-Modified-Since") + "\n";
    signature += request.getHeader("If-Match") + "\n";
    signature += request.getHeader("If-None-Match") + "\n";
    signature += request.getHeader("If-Unmodified-Since") + "\n";
    signature += request.getHeader("Range") + "\n";
    signature += canonicalized_headers + "\n";
    signature += canonicalized_resource;

    //if something wrong happens open a "telnet hostname 80", copy and paste what's the request made by curl (setting  CURLOPT_VERBOSE to 1)
    //and compare what azure is signing from what you are using
    //PrintInfo(signature);

    signature = StringUtils::base64Encode(StringUtils::hmac_sha256(signature, this->access_key));

    request.setHeader("Authorization", "SharedKey " + account_name + ":" + signature);
  }

};

}//namespace

#endif //VISUS_AZURE_CLOUD_STORAGE_

