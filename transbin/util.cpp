#include "pastream.h"

MacpingReq::MacpingReq(const int num_, void *data_, SAMPLE *samples_) :
	SendData(nullptr, data_, samples_, OTHER)
{

}