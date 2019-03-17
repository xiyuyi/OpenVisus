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

#include <Visus/NetService.h>
#include <Visus/NetSocket.h>
#include <Visus/Path.h>
#include <Visus/File.h>
#include <Visus/VisusConfig.h>
#include <Visus/Thread.h>

#include <thread>
#include <list>
#include <set>

#if WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#if VISUS_NET
  #include <curl/curl.h>

  //for sha256/sha1
  #undef  override
  #undef  final

  #include <openssl/sha.h>
  #include <openssl/evp.h>
  #include <openssl/bio.h>
  #include <openssl/buffer.h>
  #include <openssl/engine.h>
  #include <openssl/hmac.h>
  #include <openssl/evp.h>
  #include <openssl/md5.h>
  #include <openssl/opensslv.h>

  #if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)

    //HMAC_CTX_new
    static inline HMAC_CTX *HMAC_CTX_new() {
      HMAC_CTX *tmp = (HMAC_CTX *)OPENSSL_malloc(sizeof(HMAC_CTX));
      if (tmp)
        HMAC_CTX_init(tmp);
      return tmp;
    }

    //HMAC_CTX_free
    static inline void HMAC_CTX_free(HMAC_CTX *ctx) {
      if (ctx) {
        HMAC_CTX_cleanup(ctx);
        OPENSSL_free(ctx);
      }
  }
  #endif //OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)

#endif //#if VISUS_NET

namespace Visus {

#if VISUS_NET

/////////////////////////////////////////////////////////////////////////////
class NetService::Pimpl
{
public:

  //__________________________________________________________________
  class Connection
  {
  public:

    int                              id = 0;
    NetRequest                       request;
    Promise<NetResponse>             promise;
    NetResponse                      response;
    bool                             first_byte = false;

    CURLM*                           multi_handle;
    CURL*                            handle = nullptr;
    struct curl_slist*               slist = 0;
    char                             errbuf[CURL_ERROR_SIZE];
    Int64                            last_size_download = 0;
    Int64                            last_size_upload = 0;
    size_t                           buffer_offset = 0;

    //constructor
    Connection(int id_, CURLM*  multi_handle_)
      : id(id_), multi_handle(multi_handle_)
    {
      memset(errbuf, 0, sizeof(errbuf));
      this->handle = curl_easy_init();
    }

    //destructor
    ~Connection()
    {
      if (slist != nullptr) curl_slist_free_all(slist);
      curl_easy_cleanup(handle);
    }

    //setNetRequest
    void setNetRequest(NetRequest user_request, Promise<NetResponse> user_promise)
    {
      if (this->request.valid())
      {
        curl_multi_remove_handle(multi_handle, this->handle);
        curl_easy_reset(this->handle);
      }

      this->request = user_request;
      this->response = NetResponse();
      this->promise = user_promise;

      this->buffer_offset = 0;
      this->last_size_download = 0;
      this->last_size_upload = 0;
      memset(errbuf, 0, sizeof(errbuf));

      if (this->request.valid())
      {
        curl_easy_setopt(this->handle, CURLOPT_FORBID_REUSE, 1L); //not sure if this is the best option (see http://www.perlmonks.org/?node_id=925760)
        curl_easy_setopt(this->handle, CURLOPT_FRESH_CONNECT, 1L);
        curl_easy_setopt(this->handle, CURLOPT_NOSIGNAL, 1L); //otherwise crash on linux
        curl_easy_setopt(this->handle, CURLOPT_TCP_NODELAY, 1L);
        curl_easy_setopt(this->handle, CURLOPT_VERBOSE, 0L); //SET to 1L if you want to debug !

        //if you want to use TLS 1.2
        //curl_easy_setopt(this->handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);

        //VisusInfo()<<"Disabling SSL verify peer , potential security hole";
        curl_easy_setopt(this->handle, CURLOPT_SSL_VERIFYPEER, 0L);

        curl_easy_setopt(this->handle, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(this->handle, CURLOPT_HEADER, 0L);
        curl_easy_setopt(this->handle, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(this->handle, CURLOPT_USERAGENT, "visus/libcurl");
        curl_easy_setopt(this->handle, CURLOPT_ERRORBUFFER, this->errbuf);
        curl_easy_setopt(this->handle, CURLOPT_HEADERFUNCTION, HeaderFunction);
        curl_easy_setopt(this->handle, CURLOPT_WRITEFUNCTION, WriteFunction);
        curl_easy_setopt(this->handle, CURLOPT_READFUNCTION, ReadFunction);
        curl_easy_setopt(this->handle, CURLOPT_PRIVATE, this);
        curl_easy_setopt(this->handle, CURLOPT_HEADERDATA, this);
        curl_easy_setopt(this->handle, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(this->handle, CURLOPT_READDATA, this);

        String proxy = VisusConfig::readString("Configuration/NetService/proxy");
        if (!proxy.empty())
        {
          curl_easy_setopt(this->handle, CURLOPT_PROXY, proxy.c_str());
          if (int proxy_port = cint(VisusConfig::readString("Configuration/NetService/proxyport")))
            curl_easy_setopt(this->handle, CURLOPT_PROXYPORT, proxy_port);
        }

        curl_easy_setopt(this->handle, CURLOPT_URL, request.url.toString().c_str());

        //set request_headers
        if (this->slist != nullptr) curl_slist_free_all(this->slist);
        this->slist = nullptr;
        for (auto it = request.headers.begin(); it != request.headers.end(); it++)
        {
          String temp = it->first + ":" + it->second;
          this->slist = curl_slist_append(this->slist, temp.c_str());
        }

        //see https://www.redhat.com/archives/libvir-list/2012-February/msg00860.html
        //this is needed for example for VisusNetRerver that does not send "100 (Continue)"
        this->slist = curl_slist_append(this->slist, "Expect:");

        if (request.method == "DELETE")
        {
          curl_easy_setopt(this->handle, CURLOPT_CUSTOMREQUEST, "DELETE");
        }
        else if (request.method == "GET")
        {
          curl_easy_setopt(this->handle, CURLOPT_HTTPGET, 1);
        }
        else if (request.method == "HEAD")
        {
          curl_easy_setopt(this->handle, CURLOPT_HTTPGET, 1);
          curl_easy_setopt(this->handle, CURLOPT_NOBODY, 1);
        }
        else if (request.method == "POST")
        {
          curl_easy_setopt(this->handle, CURLOPT_POST, 1L);
          curl_easy_setopt(this->handle, CURLOPT_INFILESIZE, request.body ? request.body->c_size() : 0);
        }
        else if (request.method == "PUT")
        {
          curl_easy_setopt(this->handle, CURLOPT_PUT, 1);
          curl_easy_setopt(this->handle, CURLOPT_INFILESIZE, request.body ? request.body->c_size() : 0);
        }
        else
        {
          VisusAssert(false);
        }

        curl_easy_setopt(this->handle, CURLOPT_HTTPHEADER, this->slist);
        curl_multi_add_handle(multi_handle, this->handle);
      }
    }

    //HeaderFunction
    static size_t HeaderFunction(void *ptr, size_t size, size_t nmemb, Connection *connection)
    {
      connection->first_byte = true;

      if (!connection->response.body)
        connection->response.body = std::make_shared<HeapMemory>();

      size_t tot = size*nmemb;
      char* p = strchr((char*)ptr, ':');
      if (p)
      {
        String key = StringUtils::trim(String((char*)ptr, p));
        String value = StringUtils::trim(String((char*)p + 1, (char*)ptr + tot));
        if (!key.empty()) connection->response.setHeader(key, value);

        //avoid too much overhead for writeFunction function
        if (StringUtils::toLower(key) == "content-length")
        {
          int content_length = cint(value);
          connection->response.body->reserve(content_length, __FILE__, __LINE__);
        }

      }
      return(nmemb*size);
    }

    //WriteFunction (downloading data)
    static size_t WriteFunction(void *chunk, size_t size, size_t nmemb, Connection *connection)
    {
      connection->first_byte = true;

      if (!connection->response.body)
        connection->response.body = std::make_shared<HeapMemory>();

      size_t tot = size * nmemb;
      Int64 oldsize = connection->response.body->c_size();
      if (!connection->response.body->resize(oldsize + tot, __FILE__, __LINE__))
      {
        VisusAssert(false); return 0;
      }
      memcpy(connection->response.body->c_ptr() + oldsize, chunk, tot);
      ApplicationStats::net.trackReadOperation(tot);
      return tot;
    }

    //ReadFunction (uploading data)
    static size_t ReadFunction(char *chunk, size_t size, size_t nmemb, Connection *connection)
    {
      connection->first_byte = true;

      size_t& offset = connection->buffer_offset;
      size_t tot = std::min((size_t)connection->request.body->c_size() - offset, size * nmemb);
      memcpy(chunk, connection->request.body->c_ptr() + offset, tot);
      offset += tot;
      ApplicationStats::net.trackWriteOperation(tot);
      return tot;
    }

  };


  NetService* owner;
  CURLM*  multi_handle = nullptr;

  SharedPtr<std::thread> thread;

  //constructor
  Pimpl(NetService* owner_) : owner(owner_) {
  }

  //destructor
  ~Pimpl()
  {
    if (multi_handle)
      curl_multi_cleanup(multi_handle);
  }

  //start
  void start()
  {
    thread = Thread::start("Net Service Thread", [this]() {
      entryProc();
    });
  }

  //stop
  void stop() {
    owner->handleAsync(SharedPtr<NetRequest>()); //fake request to exit from the thread
    Thread::join(thread);
    thread.reset();
  }

  //createConnection
  SharedPtr<Connection> createConnection(int id)  {

    //important to create in this thread
    if (!multi_handle)
      multi_handle = curl_multi_init();

    return std::make_shared<Connection>(id, multi_handle);
  }

  //runMore
  void runMore(const std::set<Connection*>& running) 
  {
    if (running.empty())
      return;

    for (int multi_perform = CURLM_CALL_MULTI_PERFORM; multi_perform == CURLM_CALL_MULTI_PERFORM;)
    {
      int running_handles_;
      multi_perform = curl_multi_perform(multi_handle, &running_handles_);

      CURLMsg *msg; int msgs_left_;
      while ((msg = curl_multi_info_read(multi_handle, &msgs_left_)))
      {
        Connection* connection = nullptr;
        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &connection); VisusAssert(connection);

        if (msg->msg == CURLMSG_DONE)
        {
          long response_code = 0;
          curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &response_code);

          //in case the request fails before being sent, the response_code is zero. 
          connection->response.status = response_code ? response_code : HttpStatus::STATUS_BAD_REQUEST;

          if (msg->data.result != CURLE_OK)
            connection->response.setErrorMessage(String(connection->errbuf));
        }
      }
    }
  }

  //entryProc
  void entryProc()
  {
    std::vector< SharedPtr<Connection> > connections;
    for (int I = 0; I < owner->nconnections; I++)
      connections.push_back(createConnection(I));
    VisusAssert(!connections.empty());

    std::list<Connection*> available;
    for (auto connection : connections)
      available.push_back(connection.get());

    std::set<Connection*> running;
    std::deque<Int64> last_sec_connections;
    bool bExitThread = false;
    while (true)
    {
      std::ostringstream out;

      //finished?
      {
        ScopedLock lock(owner->waiting_lock);
        while (owner->waiting.empty() && running.empty())
        {
          if (bExitThread)
            return;

          owner->waiting_lock.unlock();
          owner->got_request.down();
          owner->waiting_lock.lock();
        }
      }

      //handle waiting
      {
        ScopedLock lock(owner->waiting_lock);
        Waiting still_waiting;
        for (auto it : owner->waiting)
        {
          auto request = it.first;
          auto promise = it.second;

          //request to exit ASAP (no need to execute it)
          if (!request)
          {
            ApplicationStats::num_net_jobs--;
            bExitThread = true;
            continue;
          }

          //was aborted
          if (request->aborted() || bExitThread)
          {
            request->statistics.wait_msec = (int)request->statistics.enter_t1.elapsedMsec();
            owner->waiting_lock.unlock();
            {
              auto response = NetResponse(HttpStatus::STATUS_SERVICE_UNAVAILABLE);

              request->statistics.run_msec = 0;
              promise.set_value(response);
              ApplicationStats::num_net_jobs--;
            }
            owner->waiting_lock.lock();
            continue;
          }

          if (available.empty())
          {
            still_waiting.push_back(it);
            continue;
          }

          //there is the max_connection_per_sec to respect!
          if (owner->max_connections_per_sec)
          {
            Int64 now_timestamp = Time::getTimeStamp();

            //purge too old
            while (!last_sec_connections.empty() && (now_timestamp - last_sec_connections.front()) > 1000)
              last_sec_connections.pop_front();

            if ((int)last_sec_connections.size() >= owner->max_connections_per_sec)
            {
              still_waiting.push_back(it);
              continue;
            }

            last_sec_connections.push_back(now_timestamp); ///
          }

          //don't start connection too soon, they can be soon aborted
          int wait_msec = (int)request->statistics.enter_t1.elapsedMsec();
          if (wait_msec < owner->min_wait_time)
          {
            still_waiting.push_back(it);
            continue;
          }

          //wait -> running
          Connection* connection = available.front();
          available.pop_front();
          running.insert(connection);

          request->statistics.wait_msec = wait_msec;
          request->statistics.run_t1 = Time::now();
          connection->first_byte = false;
          connection->setNetRequest(*request, promise);
          ApplicationStats::net.trackOpen();
        }
        owner->waiting = still_waiting;
      }

      //handle running
      if (!running.empty())
      {
        runMore(running);

        for (auto connection : std::set<Connection*>(running))
        {
          //run->done (since aborted)
          if (connection->request.aborted() || bExitThread)
            connection->response = NetResponse(HttpStatus::STATUS_SERVICE_UNAVAILABLE);

          //timeout (i.e. didn' receive first byte after a certain amount of seconds
          else if (!connection->first_byte && owner->connect_timeout > 0 && connection->request.statistics.run_msec >= (owner->connect_timeout * 1000))
            connection->response = NetResponse(HttpStatus::STATUS_REQUEST_TIMEOUT);

          //still running
          if (!connection->response.status)
            continue;

          connection->request.statistics.run_msec = (int)connection->request.statistics.run_t1.elapsedMsec();

          if (owner->verbose > 0 && !connection->request.aborted())
            owner->printStatistics(connection->id, connection->request, connection->response);

          connection->promise.set_value(connection->response);

          connection->setNetRequest(NetRequest(), Promise<NetResponse>());
          running.erase(connection);
          available.push_back(connection);
          ApplicationStats::num_net_jobs--;
        }
      }

      Thread::yield();
    }
  }

};

#endif

/////////////////////////////////////////////////////////////////////////////
NetService::NetService(int nconnections_,bool bVerbose) 
  : nconnections(nconnections_),verbose(bVerbose)
{
#if VISUS_NET
  this->pimpl = new Pimpl(this);
  this->pimpl->start();
#else
  ThrowException("VISUS_NET not enabled");
#endif
}

/////////////////////////////////////////////////////////////////////////////
NetService::~NetService()
{
#if VISUS_NET
  pimpl->stop();
  delete pimpl;
#endif
}


/////////////////////////////////////////////////////////////////////////////
void NetService::attach()
{
#if VISUS_NET
  int retcode = curl_global_init(CURL_GLOBAL_ALL) ;
  VisusReleaseAssert(retcode == 0);
#endif
}


/////////////////////////////////////////////////////////////////////////////
void NetService::detach()
{
#if VISUS_NET
  curl_global_cleanup();
#endif
}


///////////////////////////////////////////////////////////////////////////
String NetService::sha256(String input, String key)
{
#if VISUS_NET

  char ret[EVP_MAX_MD_SIZE];
  unsigned int  size;
  auto ctx = HMAC_CTX_new();
  HMAC_Init_ex(ctx, key.c_str(), (int)key.size(), EVP_sha256(), NULL);
  HMAC_Update(ctx, (const unsigned char*)input.c_str(), input.size());
  HMAC_Final(ctx, (unsigned char*)ret, &size);
  HMAC_CTX_free(ctx);
  return String(ret, (size_t)size);
#else
  ThrowException("VISUS_NET disabled, cannot compute sha256");
  return "";
#endif
}

///////////////////////////////////////////////////////////////////////////
String NetService::sha1(String input, String key)
{
#if VISUS_NET
  char ret[EVP_MAX_MD_SIZE];
  unsigned int size;
  auto ctx = HMAC_CTX_new();
  HMAC_Init_ex(ctx, key.c_str(), (int)key.size(), EVP_sha1(), NULL);
  HMAC_Update(ctx, (const unsigned char*)input.c_str(), input.size());
  HMAC_Final(ctx, (unsigned char*)ret, &size);
  HMAC_CTX_free(ctx);
  return String(ret, (size_t)size);
#else
  ThrowException("VISUS_NET disabled, cannot compute sha1");
  return "";
#endif
}


/////////////////////////////////////////////////////////////////////////////
Future<NetResponse> NetService::handleAsync(SharedPtr<NetRequest> request)
{
  if (request)
    request->statistics.enter_t1 = Time::now();

  ApplicationStats::num_net_jobs++;

  Promise<NetResponse> promise;
  {
    ScopedLock lock(this->waiting_lock);
    this->waiting.push_back(std::make_pair(request, promise));
  }
  this->got_request.up();
  return promise.get_future();
}

/////////////////////////////////////////////////////////////////////////////
NetResponse NetService::getNetResponse(NetRequest request)
{
  return push(SharedPtr<NetService>(),request).get();
}

/////////////////////////////////////////////////////////////////////////////
Future<NetResponse> NetService::push(SharedPtr<NetService> service, NetRequest request)
{
  if (service)
  {
    return service->handleAsync(std::make_shared<NetRequest>(request));
  }
  else
  {
    NetService service(1);
    auto future = service.handleAsync(std::make_shared<NetRequest>(request));
    NetResponse response = future.get();

    if (!response.isSuccessful() && !request.aborted())
      VisusWarning() << "request " << request.url.toString() << " failed (" << response.getErrorMessage() << ")";

    return future;
  }
}

/////////////////////////////////////////////////////////////////////////////
void NetService::printStatistics(int connection_id,const NetRequest& request,const NetResponse& response)
{
  Int64 download = response.body ? response.body->c_size() : 0;
  Int64 upload = request.body ? request.body->c_size() : 0;

  VisusInfo()  
    << request.method 
    <<" connection("<<connection_id<<")"
    << " wait(" << (request.statistics.wait_msec) << ")"
    << " running(" << (request.statistics.run_msec) << ")"
    << (download ? " download(" + StringUtils::getStringFromByteSize(download) + " - " + cstring((int)(download / (request.statistics.run_msec / 1000.0) / 1024)) + "kb/sec)" : "")
    << (upload ?   " updload("  + StringUtils::getStringFromByteSize(upload  ) + " - " + cstring((int)(upload   / (request.statistics.run_msec / 1000.0) / 1024)) + "kb/sec)" : "")
    << " status(" << response.getStatusDescription() << ")"
    << " url(" << request.url.toString() << ")";
}

} //namespace Visus


