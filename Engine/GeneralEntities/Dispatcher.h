#pragma once
#include "BufferQueue.h"
#include "HttpContext.h"
struct Dispatcher {
    BufferQueue<http_request*> requestQueue;
    BufferQueue<http_response*> responseQueue;
};
