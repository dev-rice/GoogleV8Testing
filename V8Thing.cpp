#include "V8Thing.hpp"

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

V8Thing::V8Thing() {
    V8::InitializeICU();
    // V8::InitializeExternalStartupData(argv[0]);
    platform = platform::CreateDefaultPlatform();
    V8::InitializePlatform(platform);
    V8::Initialize();
    // V8::SetFlagsFromCommandLine(&argc, argv, true);
    create_params.array_buffer_allocator = &array_buffer_allocator;
    isolate = Isolate::New(create_params);

}

V8Thing::~V8Thing() {
    isolate->Dispose();
    V8::Dispose();
    V8::ShutdownPlatform();
    delete platform;
}

int magic_number = 200;

void getMagicNumber(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    info.GetReturnValue().Set(magic_number);
}

void setMagicNumber(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {

    magic_number = value->Int32Value();
}


int V8Thing::runScript(std::string filename) {
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);

    Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
    global->Set(String::NewFromUtf8(isolate, "print"), FunctionTemplate::New(isolate, Print));

    global->SetAccessor(String::NewFromUtf8(isolate, "magic_number"), getMagicNumber, setMagicNumber);

    Local<Context> context = Context::New(isolate, NULL, global);
    if (context.IsEmpty()) {
        fprintf(stderr, "Error creating context\n");
        return 1;
    }

    Context::Scope context_scope(context);

    // Actual runScript code
    const char* str = filename.c_str();
    Local<String> file_name =
      String::NewFromUtf8(isolate, str, NewStringType::kNormal)
          .ToLocalChecked();
    Local<String> source;
    if (!ReadFile(str).ToLocal(&source)) {
        fprintf(stderr, "Error reading '%s'\n", str);
        return 0;
    }
    bool success = ExecuteString(source, file_name, false, true);
    while (platform::PumpMessageLoop(platform, isolate)) continue;

    if (success) {
        return 0;
    } else {
        return 1;
    }
    // Actual runScript ends here
}

// Reads a file into a v8 string.
MaybeLocal<String> V8Thing::ReadFile(const char* name) {
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

// Executes a string within the current v8 context.
bool V8Thing::ExecuteString(Local<String> source, Local<Value> name, bool print_result, bool report_exceptions) {

    HandleScope handle_scope(isolate);
    TryCatch try_catch(isolate);
    ScriptOrigin origin(name);
    Local<Context> context(isolate->GetCurrentContext());
    Local<Script> script;
    if (!Script::Compile(context, source, &origin).ToLocal(&script)) {
        // Print errors that happened during compilation.
        if (report_exceptions) {
            // ReportException(isolate, &try_catch);
        }
        return false;
    } else {
        Local<Value> result;
        if (!script->Run(context).ToLocal(&result)) {
            assert(try_catch.HasCaught());
            // Print errors that happened during execution.
            if (report_exceptions) {
                // ReportException(isolate, &try_catch);
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
