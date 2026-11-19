#pragma once
#include <HalStorage.h>
#include <Stream.h>

#include <cstdint>
#include <vector>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

/**
 * HTTP client utility for fetching content and downloading files.
 * Streams requests through the configured HTTP transport so large downloads
 * do not need to fit in RAM.
 */
class HttpDownloader {
 public:
  using ProgressCallback = std::function<void(size_t downloaded, size_t total)>;
  using CancelCallback = std::function<bool()>;
  // Called with each body chunk as it arrives; return false to abort. Lets a
  // streaming parser consume the response without buffering the whole body.
  using DataCallback = std::function<bool(const uint8_t* data, size_t len)>;

  enum DownloadError {
    OK = 0,
    HTTP_ERROR,
    FILE_ERROR,
    ABORTED,
    UNAUTHORIZED,
    PUBLICATION_UNCERTAIN,
  };

  static constexpr uint32_t MIN_TLS_FREE_HEAP = 40000;
  static constexpr uint32_t MIN_TLS_MAX_ALLOC = 20000;
  using Header = std::pair<std::string, std::string>;

  enum class Transport {
    ESP_HTTP,
    WOLFSSL,
  };

  struct DownloadOptions {
    explicit DownloadOptions(bool preservePartial = false, bool resumePartial = false,
                             CancelCallback shouldCancel = nullptr, size_t bufferSize = 0,
                             Transport transport = Transport::ESP_HTTP)
        : preservePartial(preservePartial),
          resumePartial(resumePartial),
          shouldCancel(std::move(shouldCancel)),
          bufferSize(bufferSize),
          transport(transport) {}

    bool preservePartial;
    bool resumePartial;
    CancelCallback shouldCancel;
    size_t bufferSize;
    Transport transport;
    // Borrowed only for this synchronous request. Basic credentials are sent
    // only to this origin; empty keeps the request URL as the credential origin.
    std::string_view authorizationOrigin;
    std::vector<Header> headers;
    std::string bearer, accept, acceptLanguage;
    int* statusOut = nullptr;
    std::string* wwwAuthenticateOut = nullptr;
    bool captureErrorBody = false;
    const char* method = "GET";
    const std::string* body = nullptr;
  };

  /**
   * Fetch text content from a URL with optional credentials.
   */
  static bool fetchUrl(const std::string& url, std::string& outContent, const std::string& username = "",
                       const std::string& password = "");

  static bool fetchUrl(const std::string& url, Stream& stream, const std::string& username = "",
                       const std::string& password = "");

  /**
   * Stream the response body to onData as it arrives, without buffering it.
   */
  static bool fetchUrl(const std::string& url, const DataCallback& onData, const std::string& username = "",
                       const std::string& password = "");

  /**
   * Stream a URL with cancellation/progress support and a detailed result.
   */
  static DownloadError streamUrl(const std::string& url, const DataCallback& onData,
                                 ProgressCallback progress = nullptr, const std::string& username = "",
                                 const std::string& password = "", DownloadOptions options = DownloadOptions());

  /**
   * Download a file to the SD card with optional credentials.
   */
  static DownloadError downloadToFile(const std::string& url, const std::string& destPath,
                                      ProgressCallback progress = nullptr, bool* cancelFlag = nullptr,
                                      const std::string& username = "", const std::string& password = "",
                                      DownloadOptions options = DownloadOptions());
  static int lastStatus();
  static bool lastAnswerRedirected();
  struct FetchOptions {
    std::string username;
    std::string password;
    std::string bearer;
    // Accept header for content negotiation (e.g. preferring OPDS 2.0 JSON
    // from servers that also speak Atom); empty sends none.
    std::string accept;
    // Accept-Language header, so servers that localize the catalog return it
    // in the reader's UI language; empty sends none.
    std::string acceptLanguage;
    // Extra headers (per-server custom headers), sent to the starting origin only.
    std::vector<Header> headers;
    int* statusOut = nullptr;
    std::string* wwwAuthenticateOut = nullptr;
    bool captureErrorBody = false;
  };

  static bool fetchUrl(const std::string& url, const DataCallback& onData, const FetchOptions& options);

  /**
   * POST an application/x-www-form-urlencoded body (e.g. an OAuth token
   * request) and collect the response body (any status) into outResponse,
   * capped at a few KB. Returns true when the response status is 2xx.
   */
  static bool postForm(const std::string& url, const std::string& formBody, std::string& outResponse,
                       int* statusOut = nullptr);


  static bool fetchUrl(const std::string& url, const DataCallback& onData, const std::string& username,
                       const std::string& password, const std::vector<Header>& headers);
  static DownloadError downloadToFile(const std::string& url, const std::string& destPath,
      ProgressCallback progress, const bool* cancelFlag, const std::string& username, const std::string& password,
      const std::vector<Header>& headers, bool downgradeRedirectsToHttp = false);
  static DownloadError postToFile(const std::string& url, const std::string& body, const std::string& contentType,
      const std::string& destPath, ProgressCallback progress = nullptr, const bool* cancelFlag = nullptr);
};
