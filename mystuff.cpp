// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include <include/v8.h>
#include <include/libplatform/libplatform.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/**
 * This sample program shows how to implement a simple javascript shell
 * based on V8.  This includes initializing V8 with command line options,
 * creating global functions, compiling and executing strings.
 *
 * For a more sophisticated shell, consider using the debug shell D8.
 */
using namespace v8;

int RunMain(Isolate* isolate, Platform* platform, int argc,
            char* argv[]);
bool ExecuteString(Isolate* isolate, Local<String> source,
                   Local<Value> name, bool print_result,
                   bool report_exceptions);
void Print(const FunctionCallbackInfo<Value>& args);
MaybeLocal<String> ReadFile(Isolate* isolate, const char* name);
void ReportException(Isolate* isolate, TryCatch* handler);

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

class Point {
public:
    Point(int x, int y) : x_(x), y_(y) { }
    int x_, y_;
};

void GetPointX(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Local<Object> self = info.Holder();
    Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
    void* ptr = wrap->Value();
    int value = static_cast<Point*>(ptr)->x_;
    info.GetReturnValue().Set(value);
}

void SetPointX(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {

    Local<Object> self = info.Holder();
    Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
    void* ptr = wrap->Value();
    static_cast<Point*>(ptr)->x_ = value->Int32Value();
}

void GetPointY(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Local<Object> self = info.Holder();
    Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
    void* ptr = wrap->Value();
    int value = static_cast<Point*>(ptr)->y_;
    info.GetReturnValue().Set(value);
}

void SetPointY(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {

    Local<Object> self = info.Holder();
    Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
    void* ptr = wrap->Value();
    static_cast<Point*>(ptr)->y_ = value->Int32Value();
}

// Global accessor stuff
int magic_number = 200;

void getMagicNumber(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    info.GetReturnValue().Set(magic_number);
}

void setMagicNumber(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {

    magic_number = value->Int32Value();
}

int main(int argc, char* argv[]) {
    V8::InitializeICU();
    V8::InitializeExternalStartupData(argv[0]);
    Platform* platform = platform::CreateDefaultPlatform();
    V8::InitializePlatform(platform);
    V8::Initialize();
    V8::SetFlagsFromCommandLine(&argc, argv, true);
    ShellArrayBufferAllocator array_buffer_allocator;
    Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = &array_buffer_allocator;
    Isolate* isolate = Isolate::New(create_params);

    int result;

    {
        Isolate::Scope isolate_scope(isolate);
        HandleScope handle_scope(isolate);

        Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
        global->Set(String::NewFromUtf8(isolate, "print", NewStringType::kNormal).ToLocalChecked(), FunctionTemplate::New(isolate, Print));

        global->SetAccessor(String::NewFromUtf8(isolate, "magic_number", NewStringType::kNormal).ToLocalChecked(), getMagicNumber, setMagicNumber);

        global->SetInternalFieldCount(1);
        global->SetAccessor(String::NewFromUtf8(isolate, "x", NewStringType::kNormal).ToLocalChecked(), GetPointX, SetPointX);
        global->SetAccessor(String::NewFromUtf8(isolate, "y", NewStringType::kNormal).ToLocalChecked(), GetPointY, SetPointY);
        // Point* p = new Point(1, 2);
        // Local<Object> obj = global->NewInstance();
        // obj->SetInternalField(0, External::New(isolate, p));

        Local<Context> context = Context::New(isolate, NULL, global);;
        if (context.IsEmpty()) {
            fprintf(stderr, "Error creating context\n");
            return 1;
        }

        Context::Scope context_scope(context);
        result = RunMain(isolate, platform, argc, argv);
    }

    isolate->Dispose();
    V8::Dispose();
    V8::ShutdownPlatform();
    delete platform;
    return result;
}
// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const String::Utf8Value& value) {
    return *value ? *value : "<string conversion failed>";
}

// The callback that is invoked by v8 whenever the JavaScript 'print'
// function is called.  Prints its arguments on stdout separated by
// spaces and ending with a newline.
void Print(const FunctionCallbackInfo<Value>& args) {
    bool first = true;
    for (int i = 0; i < args.Length(); i++) {
        HandleScope handle_scope(args.GetIsolate());
        if (first) {
            first = false;
        } else {
            printf(" ");
        }
        String::Utf8Value str(args[i]);
        const char* cstr = ToCString(str);
        printf("%s", cstr);
    }
    printf("\n");
    fflush(stdout);
}

// Reads a file into a v8 string.
MaybeLocal<String> ReadFile(Isolate* isolate, const char* name) {
    FILE* file = fopen(name, "rb");
    if (file == NULL) return MaybeLocal<String>();
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    char* chars = new char[size + 1];
    chars[size] = '\0';
    for (size_t i = 0; i < size;) {
        i += fread(&chars[i], 1, size - i, file);
        if (ferror(file)) {
            fclose(file);
            return MaybeLocal<String>();
        }
    }
    fclose(file);
    MaybeLocal<String> result = String::NewFromUtf8(isolate, chars, NewStringType::kNormal, static_cast<int>(size));
    delete[] chars;
    return result;
}

// Process remaining command line arguments and execute files
int RunMain(Isolate* isolate, Platform* platform, int argc,
            char* argv[]) {

    // Use all other arguments as names of files to load and run.
    if (argc > 2) {
        printf("Nope\n");
        return 0;
    } else {
        const char* str = argv[1];
        Local<String> file_name =
          String::NewFromUtf8(isolate, str, NewStringType::kNormal)
              .ToLocalChecked();
        Local<String> source;
        if (!ReadFile(isolate, str).ToLocal(&source)) {
            fprintf(stderr, "Error reading '%s'\n", str);
            return 0;
        }
        bool success = ExecuteString(isolate, source, file_name, false, true);
        while (platform::PumpMessageLoop(platform, isolate)) continue;
        if (!success) return 1;
    }
    return 0;
}

// Executes a string within the current v8 context.
bool ExecuteString(Isolate* isolate, Local<String> source,
                   Local<Value> name, bool print_result,
                   bool report_exceptions) {

    HandleScope handle_scope(isolate);
    TryCatch try_catch(isolate);
    ScriptOrigin origin(name);
    Local<Context> context(isolate->GetCurrentContext());
    Local<Script> script;
    if (!Script::Compile(context, source, &origin).ToLocal(&script)) {
        // Print errors that happened during compilation.
        if (report_exceptions) {
            ReportException(isolate, &try_catch);
        }
        return false;
    } else {
        Local<Value> result;
        if (!script->Run(context).ToLocal(&result)) {
            assert(try_catch.HasCaught());
            // Print errors that happened during execution.
            if (report_exceptions) {
                ReportException(isolate, &try_catch);
            }
            return false;
        } else {
            assert(!try_catch.HasCaught());
            if (print_result && !result->IsUndefined()) {
                // If all went well and the result wasn't undefined then print
                // the returned value.
                String::Utf8Value str(result);
                const char* cstr = ToCString(str);
                printf("%s\n", cstr);
            }
            return true;
        }
    }
}
void ReportException(Isolate* isolate, TryCatch* try_catch) {
    HandleScope handle_scope(isolate);
    String::Utf8Value exception(try_catch->Exception());
    const char* exception_string = ToCString(exception);

    Local<Message> message = try_catch->Message();
    if (message.IsEmpty()) {
        // V8 didn't provide any extra information about this error; just
        // print the exception.
        fprintf(stderr, "%s\n", exception_string);
    } else {
        // Print (filename):(line number): (message).
        String::Utf8Value filename(message->GetScriptOrigin().ResourceName());
        Local<Context> context(isolate->GetCurrentContext());
        const char* filename_string = ToCString(filename);
        int linenum = message->GetLineNumber(context).FromJust();
        fprintf(stderr, "%s:%i: %s\n", filename_string, linenum, exception_string);
        // Print line of source code.
        String::Utf8Value sourceline(
            message->GetSourceLine(context).ToLocalChecked());
        const char* sourceline_string = ToCString(sourceline);
        fprintf(stderr, "%s\n", sourceline_string);
        // Print wavy underline (GetUnderline is deprecated).
        int start = message->GetStartColumn(context).FromJust();
        for (int i = 0; i < start; i++) {
            fprintf(stderr, " ");
        }
        int end = message->GetEndColumn(context).FromJust();
        for (int i = start; i < end; i++) {
            fprintf(stderr, "^");
        }
        fprintf(stderr, "\n");
        Local<Value> stack_trace_string;
        if (try_catch->StackTrace(context).ToLocal(&stack_trace_string) &&
            stack_trace_string->IsString() &&
            Local<String>::Cast(stack_trace_string)->Length() > 0) {

            String::Utf8Value stack_trace(stack_trace_string);
            const char* stack_trace_string = ToCString(stack_trace);
            fprintf(stderr, "%s\n", stack_trace_string);
        }
    }
}
