#pragma once

#include <include/v8.h>
#include <include/libplatform/libplatform.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

using namespace v8;

const char* ToCString(const String::Utf8Value& value);

class ShellArrayBufferAllocator : public ArrayBuffer::Allocator {
public:
    virtual void* Allocate(size_t length) {
        void* data = AllocateUninitialized(length);
        return data == NULL ? data : memset(data, 0, length);
    }

    virtual void* AllocateUninitialized(size_t length) {
        return malloc(length);
    }

    virtual void Free(void* data, size_t) {
        free(data);
    }
};

class V8Thing {
public:
    V8Thing();
    ~V8Thing();

    int runScript(std::string filename);
private:
    MaybeLocal<String> ReadFile(const char* name);
    bool ExecuteString(Local<String> source, Local<Value> name, bool print_result, bool report_exceptions);


    Platform* platform;
    ShellArrayBufferAllocator array_buffer_allocator;
    Isolate::CreateParams create_params;
    Isolate* isolate;

};
