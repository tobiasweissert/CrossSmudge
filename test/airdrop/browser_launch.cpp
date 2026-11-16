#include "BrowserLaunch.h"
#include <cassert>
#include <cstring>
#include <memory>
using namespace airdrop::browser;
int main(){
  auto owned=std::unique_ptr<LaunchRequest>(new LaunchRequest);
  assert(!owned->prepare("http://example.com/"));
  assert(owned->prepare("https://example.com/a#fragment"));
  LaunchHandle first(std::move(owned));LaunchHandle second=first;
  assert(first->state()==LaunchState::Pending);
  auto token=first->beginAttempt();assert(token);
  assert(first->state()==LaunchState::Offered);
  assert(!first->acknowledge(token));
  assert(std::strcmp(second->take(token),"https://example.com/a#fragment")==0);
  assert(!second->take(token));
  assert(first->failAttempt(token,LaunchError::ScriptFailed));
  assert(first->state()==LaunchState::Pending);
  auto retry=first->beginAttempt();assert(retry&&retry!=token);
  assert(!second->acknowledge(token));assert(!second->take(token));
  assert(second->take(retry));assert(second->acknowledge(retry));
  assert(first->state()==LaunchState::Acknowledged);
  assert(!first->beginAttempt());assert(!first->acknowledge(retry));
  auto cancelled=std::unique_ptr<LaunchRequest>(new LaunchRequest);assert(cancelled->prepare("https://example.com/"));
  LaunchHandle third(std::move(cancelled));auto stale=third->beginAttempt();third->cancel();assert(!third->take(stale));assert(!third->acknowledge(stale));
}
