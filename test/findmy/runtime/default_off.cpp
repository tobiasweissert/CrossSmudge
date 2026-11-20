#include "findmy/Firmware.h"
#include <cassert>
int main(){
 crosscub::findmy::firmware::poll();
 assert(!crosscub::findmy::firmware::standby());
 assert(crosscub::findmy::firmware::beforeAirDrop());
 assert(crosscub::findmy::firmware::beforeSleep());
}
